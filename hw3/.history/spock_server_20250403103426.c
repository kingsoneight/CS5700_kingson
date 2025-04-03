/******************************************************************************
 * spock_server.c
 *
 * A multi-player "Rock, Paper, Scissors, Lizard, Spock" server that:
 *  1) Accepts a port number and the total number of players to wait for.
 *  2) Waits for (numPlayers - 1) clients to connect (the server itself acts
 *     as Player #1, if you choose to let the server also play).
 *  3) After all players have connected, it repeatedly collects moves from
 *     everyone and determines a winner (or tie).
 *  4) Supports commands like RESET (T), QUIT (Q), etc.
 *
 * Usage:
 *   ./spock_server <port> <numPlayers>
 * Example:
 *   ./spock_server 5555 3   # Server listens on port 5555, expects 3 total players
 *
 * For a simpler approach, in many classroom assignments, you might run the
 * server in "referee-only" mode (it doesn't play). That's a design choice.
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

#define MAX_PLAYERS 5
#define BUF_SIZE 1024

/* Moves enumeration */
typedef enum
{
    MOVE_ROCK,
    MOVE_PAPER,
    MOVE_SCISSORS,
    MOVE_LIZARD,
    MOVE_SPOCK,
    MOVE_INVALID
} Move;

/* Function prototypes */
static int start_server(int port);
static void handle_game(int *client_fds, int numPlayers);
static Move char_to_move(char c);
static const char *move_to_string(Move m);
static int determine_multiplayer_winner(Move moves[], int numPlayers);

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <port> <numPlayers>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    int numPlayers = atoi(argv[2]);
    if (numPlayers < 2 || numPlayers > MAX_PLAYERS)
    {
        fprintf(stderr, "numPlayers must be between 2 and %d.\n", MAX_PLAYERS);
        exit(1);
    }

    int server_fd = start_server(port);
    if (server_fd < 0)
    {
        fprintf(stderr, "Failed to start server on port %d\n", port);
        return 1;
    }

    printf("Server listening on port %d, waiting for %d total players...\n",
           port, numPlayers);

    /* We will accept (numPlayers) connections if we assume the server does NOT play
     * or (numPlayers - 1) if the server is also a player.
     * For demonstration, let's assume the server is NOT playing and we want exactly
     * numPlayers to be remote players. Adjust to your preference.
     */
    int client_fds[MAX_PLAYERS] = {-1, -1, -1, -1, -1};
    int connected_count = 0;
    while (connected_count < numPlayers)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0)
        {
            perror("accept");
            close(server_fd);
            return 1;
        }

        printf("New client connected (fd=%d). [%d/%d]\n",
               client_fd, connected_count + 1, numPlayers);

        client_fds[connected_count] = client_fd;
        connected_count++;
    }

    /* At this point, we have numPlayers connected. We can close the server
     * listening socket if we aren't allowing more players to join.
     */
    close(server_fd);

    /* Handle game logic (infinite rounds until QUIT) */
    handle_game(client_fds, numPlayers);

    /* Cleanup */
    for (int i = 0; i < numPlayers; i++)
    {
        close(client_fds[i]);
    }
    return 0;
}

/* Starts listening socket on given port. Returns the server_fd or -1 on error. */
static int start_server(int port)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        close(server_fd);
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        close(server_fd);
        return -1;
    }

    return server_fd;
}

/* Orchestrates the multi-player RPSLS game.
 * In each round:
 *  1) We wait to receive a "MOVE:X", "RESET", or "QUIT" from each player.
 *  2) If any player quits, we broadcast QUIT and stop.
 *  3) If we get RESET from any player, we reset scores.
 *  4) Once we have all moves, we compute the winner(s) and broadcast results+scores.
 */
static void handle_game(int *client_fds, int numPlayers)
{
    int scores[MAX_PLAYERS];
    memset(scores, 0, sizeof(scores));

    char buffer[BUF_SIZE];
    fd_set read_fds;

    /* Outer loop: each iteration = one round (unless a RESET triggers a new round). */
    while (1)
    {
        /* Moves array for this round.  Set all to INVALID initially. */
        Move moves[MAX_PLAYERS];
        for (int i = 0; i < numPlayers; i++)
        {
            moves[i] = MOVE_INVALID;
        }

        int moves_received = 0;
        int round_over = 0;

        /* We'll add these two flags to know how the round ended. */
        int someone_quit = 0;
        int someone_reset = 0;

        /* Inner loop: gather moves/commands from all players until we have them all
         * or until something ends the round (QUIT or RESET).
         */
        while (moves_received < numPlayers && !round_over)
        {
            FD_ZERO(&read_fds);

            int max_fd = -1;
            for (int i = 0; i < numPlayers; i++)
            {
                FD_SET(client_fds[i], &read_fds);
                if (client_fds[i] > max_fd)
                    max_fd = client_fds[i];
            }

            int ret = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
            if (ret < 0)
            {
                perror("select");
                /* If select fails, we can consider ending the session. */
                return;
            }

            /* Check each client for activity */
            for (int i = 0; i < numPlayers; i++)
            {
                int fd = client_fds[i];
                if (!FD_ISSET(fd, &read_fds))
                {
                    continue;
                }

                memset(buffer, 0, sizeof(buffer));
                int n = recv(fd, buffer, BUF_SIZE - 1, 0);
                if (n <= 0)
                {
                    /* A client disconnected or error -> end game. */
                    printf("Player %d disconnected. Ending game.\n", i + 1);
                    someone_quit = 1;
                    round_over = 1;
                    break;
                }

                buffer[n] = '\0';
                // Parse
                if (strncmp(buffer, "QUIT", 4) == 0)
                {
                    printf("Player %d requested QUIT.\n", i + 1);
                    someone_quit = 1;
                    round_over = 1;
                    break;
                }
                else if (strncmp(buffer, "RESET", 5) == 0)
                {
                    printf("Player %d requested RESET.\n", i + 1);
                    /* Reset all scores */
                    for (int k = 0; k < numPlayers; k++)
                    {
                        scores[k] = 0;
                    }
                    /* Broadcast RESET to all */
                    for (int k = 0; k < numPlayers; k++)
                    {
                        send(client_fds[k], "RESET", 5, 0);
                    }
                    someone_reset = 1;
                    round_over = 1; // This ends the current round
                    break;
                }
                else if (strncmp(buffer, "MOVE:", 5) == 0)
                {
                    char c = buffer[5];
                    Move m = char_to_move(c);
                    if (m != MOVE_INVALID && moves[i] == MOVE_INVALID)
                    {
                        moves[i] = m;
                        moves_received++;
                        printf("Got move from player %d: %s\n",
                               i + 1, move_to_string(m));
                    }
                    // else ignore invalid or duplicate moves from same player
                }
                else
                {
                    // Unknown or partial command
                    printf("Player %d sent unknown cmd: %s\n", i + 1, buffer);
                }

                if (round_over)
                {
                    break; // break out of for-loop checking clients
                }
            } // end for each client
        } // end while (moves_received < numPlayers && !round_over)

        /* Now that the round is over (because we have all moves or a command ended it),
         * decide how to proceed.
         */
        if (someone_quit)
        {
            // A QUIT or disconnection => broadcast QUIT to everyone & end session
            for (int k = 0; k < numPlayers; k++)
            {
                send(client_fds[k], "QUIT", 4, 0);
            }
            break; // break out of the outer loop, game session ends
        }

        if (someone_reset)
        {
            // We do NOT break the entire loop; we only skip computing a winner
            // and start a fresh round. So we do "continue".
            // The updated (zeroed) scores are already broadcast.
            continue;
        }

        /* If the round ended because we collected all moves,
         * we compute the winner and broadcast the results.
         */
        if (moves_received == numPlayers)
        {
            int winners[MAX_PLAYERS];
            int numWinners = 0;
            determine_multiplayer_winners(moves, numPlayers, winners, &numWinners);

            /* Build and broadcast the RESULT message */
            char result_msg[BUF_SIZE];
            char moves_part[BUF_SIZE];
            moves_part[0] = '\0';
            for (int i = 0; i < numPlayers; i++)
            {
                char temp[32];
                snprintf(temp, sizeof(temp), "%s%s",
                         (i == 0) ? "" : ",",
                         move_to_string(moves[i]));
                strcat(moves_part, temp);
            }

            char scores_part[BUF_SIZE];
            scores_part[0] = '\0';
            for (int i = 0; i < numPlayers; i++)
            {
                char temp[32];
                snprintf(temp, sizeof(temp), "%s%d",
                         (i == 0) ? "" : ",",
                         scores[i]);
                strcat(scores_part, temp);
            }

            snprintf(result_msg, sizeof(result_msg),
                     "RESULT:%d:%s:%s", winners, moves_part, scores_part);

            for (int i = 0; i < numPlayers; i++)
            {
                send(client_fds[i], result_msg, strlen(result_msg), 0);
            }
        }

        // We now loop back for another round unless a QUIT triggers above.
    } // end outer while(1)

    printf("Game session ended.\n");
}

/* Convert char to Move enum. */
static Move char_to_move(char c)
{
    switch (c)
    {
    case 'R':
    case 'r':
        return MOVE_ROCK;
    case 'P':
    case 'p':
        return MOVE_PAPER;
    case 'S':
    case 's':
        return MOVE_SCISSORS;
    case 'L':
    case 'l':
        return MOVE_LIZARD;
    case 'K':
    case 'k':
        return MOVE_SPOCK;
    default:
        return MOVE_INVALID;
    }
}

/* Convert Move enum to string. */
static const char *move_to_string(Move m)
{
    switch (m)
    {
    case MOVE_ROCK:
        return "Rock";
    case MOVE_PAPER:
        return "Paper";
    case MOVE_SCISSORS:
        return "Scissors";
    case MOVE_LIZARD:
        return "Lizard";
    case MOVE_SPOCK:
        return "Spock";
    default:
        return "Invalid";
    }
}

/* Helper function to check if m1 beats m2 in RPSLS logic */
static int beats(Move m1, Move m2)
{
    /* Return 1 if m1 beats m2, else 0 */
    if ((m1 == MOVE_PAPER && (m2 == MOVE_ROCK || m2 == MOVE_SPOCK)) ||
        (m1 == MOVE_SCISSORS && (m2 == MOVE_PAPER || m2 == MOVE_LIZARD)) ||
        (m1 == MOVE_SPOCK && (m2 == MOVE_SCISSORS || m2 == MOVE_ROCK)) ||
        (m1 == MOVE_ROCK && (m2 == MOVE_SCISSORS || m2 == MOVE_LIZARD)) ||
        (m1 == MOVE_LIZARD && (m2 == MOVE_SPOCK || m2 == MOVE_PAPER)))
    {
        return 1;
    }
    return 0;
}

/*
 * Checks all players' moves to find all "dominant" moves.
 * A move M is "dominant" if:
 *   1) It beats at least one other move (optional condition, to avoid weird edge cases)
 *   2) It is NOT beaten by any other move.
 * Each "dominant" player's index is stored in winners[], and *pNumWinners is set to how many.
 * If no dominant moves => *pNumWinners=0 => tie.
 */
static void determine_multiplayer_winners(Move moves[], int numPlayers,
                                          int winners[], int *pNumWinners)
{
// We'll define a small "beats" helper for clarity:
// (In C, just define a static function or inline if you prefer.)
auto_beats: /* PSEUDOCODE ONLY – see below for pure C version! */

    int countWinners = 0;

    for (int i = 0; i < numPlayers; i++)
    {
        if (moves[i] == MOVE_INVALID)
        {
            // skip players who didn't send a valid move
            continue;
        }

        int is_beaten = 0;
        int at_least_one_beat = 0;

        for (int j = 0; j < numPlayers; j++)
        {
            if (i == j || moves[j] == MOVE_INVALID)
            {
                continue;
            }
            // if (beats(moves[j], moves[i])) => I'm beaten
            // if (beats(moves[i], moves[j])) => I beat at least one
        }

        if (!is_beaten && at_least_one_beat)
        {
            // This player's move is "dominant"
            winners[countWinners++] = i;
        }
    }

    *pNumWinners = countWinners;
}
