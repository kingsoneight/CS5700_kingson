/******************************************************************************
 * spock_server.c
 *
 * A multi-player (up to 5) "Rock, Paper, Scissors, Lizard, Spock" server that:
 *   1) Accepts <port> and <numPlayers> from the command line.
 *   2) Listens for exactly numPlayers clients to connect (server is only a ref).
 *   3) Runs multiple rounds:
 *       - Each player sends a command: either "MOVE:<char>", "QUIT", or "RESET".
 *       - On QUIT, the entire game ends for all.
 *       - On RESET, the current scores are zeroed, and a new round begins.
 *       - Once all players have sent valid moves, the server finds all
 *         "dominant" moves. Each player that played a dominant move gains +1.
 *       - The server broadcasts the round result with "RESULT:..."
 *   4) Continues until a QUIT or disconnection occurs.
 *
 * Usage example:
 *   ./spock_server 5555 3
 *   => Listens on TCP port 5555, waits for 3 clients.
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
static void usage(const char *prog);
static int start_server(int port);
static void handle_game(int *client_fds, int numPlayers);
static Move char_to_move(char c);
static const char *move_to_string(Move m);
static int beats(Move m1, Move m2);

/*
 * determine_multiplayer_winners:
 *   Identifies all "dominant" moves in this round.
 *   A move M_i is "dominant" if:
 *      (1) It beats at least one other move in the round.
 *      (2) It is NOT beaten by any other move in the round.
 *   Everyone who played M_i gets +1 to score.
 *
 *   @param moves[]: array of size numPlayers with each player's Move.
 *   @param winners[]: out-parameter to store the indices (0-based) of winners
 *   @param pNumWinners: out-parameter storing how many winners found
 */
static void determine_multiplayer_winners(Move moves[], int numPlayers,
                                          int winners[], int *pNumWinners);

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        usage(argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    int numPlayers = atoi(argv[2]);

    if (numPlayers < 1 || numPlayers > MAX_PLAYERS)
    {
        fprintf(stderr, "numPlayers must be between 1 and %d.\n", MAX_PLAYERS);
        exit(1);
    }

    int server_fd = start_server(port);
    if (server_fd < 0)
    {
        fprintf(stderr, "Error: could not start server on port %d.\n", port);
        return 1;
    }

    printf("[Server] Listening on port %d, expecting %d clients...\n",
           port, numPlayers);

    int client_fds[MAX_PLAYERS];
    memset(client_fds, -1, sizeof(client_fds));

    /* Accept exactly numPlayers connections */
    int connected_count = 0;
    while (connected_count < numPlayers)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int cfd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (cfd < 0)
        {
            perror("accept");
            close(server_fd);
            return 1;
        }
        client_fds[connected_count++] = cfd;
        printf("[Server] New client connected (fd=%d). [%d/%d]\n",
               cfd, connected_count, numPlayers);
    }

    /* We can close the listening socket now if we only want these numPlayers. */
    close(server_fd);

    /* Run the game loop (multiple rounds) until someone quits or disconnects. */
    handle_game(client_fds, numPlayers);

    /* Cleanup */
    for (int i = 0; i < numPlayers; i++)
    {
        if (client_fds[i] >= 0)
        {
            close(client_fds[i]);
        }
    }

    return 0;
}

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <port> <numPlayers>\n", prog);
    fprintf(stderr, "Example: %s 5555 3\n", prog);
}

/* start_server: create a listening socket on the specified port. */
static int start_server(int port)
{
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0)
    {
        perror("socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        close(sfd);
        return -1;
    }

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons(port);

    if (bind(sfd, (struct sockaddr *)&srv, sizeof(srv)) < 0)
    {
        perror("bind");
        close(sfd);
        return -1;
    }

    if (listen(sfd, 5) < 0)
    {
        perror("listen");
        close(sfd);
        return -1;
    }
    return sfd;
}

/*
 * handle_game:
 *   Main "round" loop: repeatedly gather moves or commands from each player,
 *   then compute winners or handle QUIT/RESET.
 */
static void handle_game(int *client_fds, int numPlayers)
{
    int scores[MAX_PLAYERS];
    memset(scores, 0, sizeof(scores));

    char buffer[BUF_SIZE];
    fd_set read_fds;

    while (1)
    {
        /* We track each player's move for this round in moves[] */
        Move moves[MAX_PLAYERS];
        for (int i = 0; i < numPlayers; i++)
        {
            moves[i] = MOVE_INVALID;
        }

        int moves_received = 0;

        int round_over = 0;    // if we got all moves or a special command
        int someone_quit = 0;  // if a QUIT or disconnect occurs
        int someone_reset = 0; // if a RESET command occurs

        //----- Gather commands/moves from all players -----
        while (moves_received < numPlayers && !round_over)
        {
            FD_ZERO(&read_fds);
            int max_fd = -1;
            for (int i = 0; i < numPlayers; i++)
            {
                FD_SET(client_fds[i], &read_fds);
                if (client_fds[i] > max_fd)
                {
                    max_fd = client_fds[i];
                }
            }

            int ret = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
            if (ret < 0)
            {
                perror("select");
                return;
            }

            /* Check for activity on each player's socket */
            for (int i = 0; i < numPlayers; i++)
            {
                int fd = client_fds[i];
                if (!FD_ISSET(fd, &read_fds))
                {
                    continue;
                }

                memset(buffer, 0, BUF_SIZE);
                int n = recv(fd, buffer, BUF_SIZE - 1, 0);
                if (n <= 0)
                {
                    // player disconnected or error => end entire session
                    printf("[Server] Player %d disconnected. Ending game.\n", i + 1);
                    someone_quit = 1;
                    round_over = 1;
                    break;
                }

                buffer[n] = '\0';
                // parse command:
                if (strncmp(buffer, "QUIT", 4) == 0)
                {
                    printf("[Server] Player %d requested QUIT.\n", i + 1);
                    someone_quit = 1;
                    round_over = 1;
                    break;
                }
                else if (strncmp(buffer, "RESET", 5) == 0)
                {
                    printf("[Server] Player %d requested RESET.\n", i + 1);
                    // zero out all scores
                    for (int k = 0; k < numPlayers; k++)
                    {
                        scores[k] = 0;
                    }
                    // broadcast RESET
                    for (int k = 0; k < numPlayers; k++)
                    {
                        send(client_fds[k], "RESET", 5, 0);
                    }
                    someone_reset = 1;
                    round_over = 1;
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
                        printf("[Server] Player %d => %s\n", i + 1, move_to_string(m));
                    }
                    // else ignore invalid or duplicate move
                }
                else
                {
                    // unknown command
                    printf("[Server] Player %d sent unknown: %s\n", i + 1, buffer);
                }

                if (round_over)
                {
                    break;
                }
            } // end for each player
        } // end while collecting moves

        //----- Decide how round ended -----
        if (someone_quit)
        {
            // broadcast QUIT
            for (int i = 0; i < numPlayers; i++)
            {
                send(client_fds[i], "QUIT", 4, 0);
            }
            break; // entire game ends
        }

        if (someone_reset)
        {
            // do NOT break the entire game => we skip winner calc & start new round
            continue;
        }

        // If we got all moves, compute multi-winner(s)
        if (moves_received == numPlayers)
        {
            int winners[MAX_PLAYERS];
            int numWinners = 0;
            determine_multiplayer_winners(moves, numPlayers, winners, &numWinners);

            if (numWinners == 0)
            {
                printf("[Server] Round ends in a tie.\n");
            }
            else
            {
                printf("[Server] Dominant move(s): ");
                for (int w = 0; w < numWinners; w++)
                {
                    scores[winners[w]]++;
                    printf("Player %d ", (winners[w] + 1));
                }
                printf("\n");
            }

            // Build & broadcast the RESULT message
            // Format example: RESULT:numWinners:move0,move1:score0,score1
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

            // Also list the winners
            char winners_part[BUF_SIZE];
            winners_part[0] = '\0';
            for (int i = 0; i < numWinners; i++)
            {
                char temp[16];
                snprintf(temp, sizeof(temp), "%s%d",
                         (i == 0) ? "" : ",",
                         (winners[i] + 1));
                strcat(winners_part, temp);
            }

            char result_msg[BUF_SIZE];
            snprintf(result_msg, sizeof(result_msg),
                     "RESULT:%s:%s:%s", winners_part, moves_part, scores_part);

            for (int i = 0; i < numPlayers; i++)
            {
                send(client_fds[i], result_msg, strlen(result_msg), 0);
            }
        }

        // Then we loop around to start a new round (unless QUIT ended us)
    } // end while(1)

    printf("[Server] Game session ended.\n");
}

/* char_to_move: map single character to Move enum. */
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

/* move_to_string: return human-readable name of move. */
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

/* "beats": Return 1 if m1 beats m2 under RPSLS rules, else 0. */
static int beats(Move m1, Move m2)
{
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
 * determine_multiplayer_winners:
 *   Find all "dominant" moves. Each dominant move's players get +1.
 *   A move is "dominant" if it is not beaten by any other move, and it beats
 *   at least one other move in this round (so it’s not a pointless same-same scenario).
 *
 *   winners[] is an OUT array of indices of players who have a dominant move.
 *   *pNumWinners is how many entries are in winners[].
 */
static void determine_multiplayer_winners(Move moves[], int numPlayers,
                                          int winners[], int *pNumWinners)
{
    *pNumWinners = 0;
    // We'll find the set of unique moves that are dominant. Then see which players used them.

    // 1) Identify which moves are dominant
    int isDominant[MAX_PLAYERS]; // which player has a dominant move
    memset(isDominant, 0, sizeof(isDominant));

    for (int i = 0; i < numPlayers; i++)
    {
        if (moves[i] == MOVE_INVALID)
            continue;

        int i_is_beaten = 0;
        int i_beats_any = 0;
        for (int j = 0; j < numPlayers; j++)
        {
            if (i == j || moves[j] == MOVE_INVALID)
            {
                continue;
            }
            if (beats(moves[j], moves[i]))
            {
                // j's move beats i's move => i can't be dominant
                i_is_beaten = 1;
                break;
            }
            if (beats(moves[i], moves[j]))
            {
                i_beats_any = 1;
            }
        }

        if (!i_is_beaten && i_beats_any)
        {
            isDominant[i] = 1; // player i’s move is dominant
        }
    }

    // 2) Fill winners[] with all i for which isDominant[i] = 1
    int count = 0;
    for (int i = 0; i < numPlayers; i++)
    {
        if (isDominant[i])
        {
            winners[count++] = i;
        }
    }
    *pNumWinners = count;
}
