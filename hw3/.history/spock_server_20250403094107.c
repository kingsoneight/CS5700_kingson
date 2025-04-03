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

    while (1)
    {
        /* We will gather each player's move for this round.
         * If we do a synchronous approach, let's do:
         *   move[i] = MOVE_INVALID until we get a valid move from player i
         */
        Move moves[MAX_PLAYERS];
        for (int i = 0; i < numPlayers; i++)
        {
            moves[i] = MOVE_INVALID;
        }

        int moves_received = 0;
        int round_over = 0; /* will be set if a QUIT ends everything, etc. */

        /* We'll loop until we get all moves or a game-ending command. */
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
                return;
            }

            /* Check each client for activity */
            for (int i = 0; i < numPlayers; i++)
            {
                int fd = client_fds[i];
                if (FD_ISSET(fd, &read_fds))
                {
                    memset(buffer, 0, sizeof(buffer));
                    int n = recv(fd, buffer, BUF_SIZE - 1, 0);
                    if (n <= 0)
                    {
                        /* A client disconnected or error -> end game. */
                        printf("Player %d disconnected. Ending game.\n", i + 1);
                        round_over = 1;
                        /* Broadcast QUIT to everyone */
                        for (int k = 0; k < numPlayers; k++)
                        {
                            if (client_fds[k] >= 0)
                            {
                                send(client_fds[k], "QUIT", 4, 0);
                            }
                        }
                        break;
                    }

                    buffer[n] = '\0';
                    // Parse
                    if (strncmp(buffer, "QUIT", 4) == 0)
                    {
                        printf("Player %d requested QUIT.\n", i + 1);
                        /* Broadcast QUIT to everyone */
                        for (int k = 0; k < numPlayers; k++)
                        {
                            send(client_fds[k], "QUIT", 4, 0);
                        }
                        round_over = 1; // end entire game
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
                        /* We'll skip the rest of the round. Everyone must re-move. */
                        round_over = 1;
                        break;
                    }
                    else if (strncmp(buffer, "MOVE:", 5) == 0)
                    {
                        char c = buffer[5];
                        Move m = char_to_move(c);
                        if (m == MOVE_INVALID)
                        {
                            // ignore or handle error
                            printf("Invalid move from player %d.\n", i + 1);
                        }
                        else
                        {
                            if (moves[i] == MOVE_INVALID)
                            {
                                /* A new valid move from player i */
                                moves[i] = m;
                                moves_received++;
                                printf("Got move from player %d: %s\n",
                                       i + 1, move_to_string(m));
                            }
                            // If they already sent a move, we ignore or override, up to you
                        }
                    }
                    else
                    {
                        // Unknown command or partial data
                        printf("Player %d sent unknown: %s\n", i + 1, buffer);
                    }
                }
            } // end for each player fd
        } // end while moves_received < numPlayers

        if (round_over)
        {
            // means either QUIT or RESET ended the round (or disconnection)
            if (moves_received < numPlayers)
            {
                // We do not compute a result if the round ended abruptly.
                // If it's QUIT -> break out entirely
                // If it was RESET -> we continue to next round
            }
            // If QUIT, let's break from outer loop:
            break;
        }

        /* If we reach here, we have moves from all players => compute winner(s). */
        int winning_move_index = determine_multiplayer_winner(moves, numPlayers);
        /* If winning_move_index == -1, it means tie. Otherwise that index is the unique winner.
         * For a more advanced multi-winner scenario, you might store a list of winners.
         */

        if (winning_move_index >= 0)
        {
            // There's exactly one winner
            scores[winning_move_index]++;
            printf("Player %d wins the round!\n", winning_move_index + 1);
        }
        else
        {
            printf("Round ended in a tie.\n");
        }

        /* Broadcast result to each player:
         * We'll send something like: "RESULT:<winnerIndexOr-1>:move0,move1,move2:score0,score1,score2"
         */
        char result_msg[BUF_SIZE];
        // Build moves portion
        char moves_part[BUF_SIZE];
        moves_part[0] = '\0';
        for (int i = 0; i < numPlayers; i++)
        {
            char temp[32];
            snprintf(temp, sizeof(temp), "%s%s", i == 0 ? "" : ",", move_to_string(moves[i]));
            strcat(moves_part, temp);
        }
        // Build scores portion
        char scores_part[BUF_SIZE];
        scores_part[0] = '\0';
        for (int i = 0; i < numPlayers; i++)
        {
            char temp[32];
            snprintf(temp, sizeof(temp), "%s%d", i == 0 ? "" : ",", scores[i]);
            strcat(scores_part, temp);
        }
        snprintf(result_msg, sizeof(result_msg),
                 "RESULT:%d:%s:%s", winning_move_index, moves_part, scores_part);

        for (int i = 0; i < numPlayers; i++)
        {
            send(client_fds[i], result_msg, strlen(result_msg), 0);
        }
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

static int determine_multiplayer_winner(Move moves[], int numPlayers)
{
    /*
     * For each player's move 'mi', check if it is "dominant" =
     *   - 'mi' beats at least one move (otherwise it's not interesting)
     *   - no other move beats 'mi'.
     *
     * If exactly one player's move passes that test, we return that player's index.
     * If none or multiple do, return -1 (tie).
     */

    int possible_winner_index = -1;

    for (int i = 0; i < numPlayers; i++)
    {
        Move mi = moves[i];
        if (mi == MOVE_INVALID)
        {
            continue; // skip invalid
        }

        int is_beaten = 0;
        int at_least_one_beat = 0;

        for (int j = 0; j < numPlayers; j++)
        {
            if (i == j)
                continue; // don't compare to self
            Move mj = moves[j];
            if (mj == MOVE_INVALID)
                continue;

            if (beats(mj, mi))
            {
                // someone else's move mj beats my mi
                is_beaten = 1;
                break;
            }
            if (beats(mi, mj))
            {
                // my mi beats someone else's mj
                at_least_one_beat = 1;
            }
        }

        if (!is_beaten && at_least_one_beat)
        {
            // candidate for unique winner
            if (possible_winner_index == -1)
            {
                possible_winner_index = i; // first candidate
            }
            else
            {
                // we already have a candidate => multiple "dominant" moves => tie
                return -1;
            }
        }
    }

    return possible_winner_index;
}
