/******************************************************************************
 * spock_server.c
 *
 * A two‑player "Rock, Paper, Scissors, Lizard, Spock" server.
 *
 * Usage:
 *   gcc -Wall -Wextra -O2 spock_server.c -o spock_server
 *   ./spock_server [port]   # default port is 5131
 *
 * Protocol:
 *   - Server → Client:
 *       INFO:<text>           # welcome, rules, round start, prompts
 *       RESET                 # scores reset
 *       QUIT                  # game over
 *       RESULT:<p1>:<p2>:<s1>:<s2>
 *
 *   - Client → Server:
 *       MOVE:<R|P|S|L|K>
 *       RESET
 *       QUIT
 ******************************************************************************/

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <ctype.h>      // for toupper()
 #include <unistd.h>
 #include <arpa/inet.h>
 #include <sys/socket.h>
 #include <sys/select.h>
 
 #define DEFAULT_PORT 5131
 #define BUF_SIZE      1024
 
 /* Enumeration of possible moves */
 typedef enum {
     MOVE_ROCK,
     MOVE_PAPER,
     MOVE_SCISSORS,
     MOVE_LIZARD,
     MOVE_SPOCK,
     MOVE_INVALID
 } Move;
 
 /* Convert a character to the corresponding Move enum */
 static Move char_to_move(char c) {
     c = toupper((unsigned char)c);
     switch (c) {
         case 'R': return MOVE_ROCK;
         case 'P': return MOVE_PAPER;
         case 'S': return MOVE_SCISSORS;
         case 'L': return MOVE_LIZARD;
         case 'K': return MOVE_SPOCK;
         default:  return MOVE_INVALID;
     }
 }
 
 /* Convert a Move enum to its single-character code */
 static char move_to_char(Move m) {
     const char *map = "RPSLK";
     return (m >= 0 && m < 5) ? map[m] : 'X';
 }
 
 /* Return 1 if m1 beats m2, 0 otherwise */
 static int beats(Move m1, Move m2) {
     if (m1==MOVE_ROCK     && (m2==MOVE_SCISSORS || m2==MOVE_LIZARD))  return 1;
     if (m1==MOVE_PAPER    && (m2==MOVE_ROCK     || m2==MOVE_SPOCK ))  return 1;
     if (m1==MOVE_SCISSORS && (m2==MOVE_PAPER    || m2==MOVE_LIZARD))  return 1;
     if (m1==MOVE_LIZARD   && (m2==MOVE_PAPER    || m2==MOVE_SPOCK ))  return 1;
     if (m1==MOVE_SPOCK    && (m2==MOVE_ROCK     || m2==MOVE_SCISSORS)) return 1;
     return 0;
 }
 
 /* Print usage and exit */
 static void usage(const char *prog) {
     fprintf(stderr,
         "Usage: %s [port]\n"
         "  port: optional TCP port (default %d)\n",
         prog, DEFAULT_PORT);
     exit(1);
 }
 
 int main(int argc, char *argv[]) {
     int port = DEFAULT_PORT;
     if (argc == 2) {
         port = atoi(argv[1]);
         if (port <= 0) port = DEFAULT_PORT;
     } else if (argc > 2) {
         usage(argv[0]);
     }
 
     /* Create listening socket */
     int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
     if (listen_fd < 0) { perror("socket"); exit(1); }
     int opt = 1;
     setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
 
     struct sockaddr_in addr = {0};
     addr.sin_family      = AF_INET;
     addr.sin_addr.s_addr = INADDR_ANY;
     addr.sin_port        = htons(port);
 
     if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
         perror("bind"); close(listen_fd); exit(1);
     }
     if (listen(listen_fd, 1) < 0) {
         perror("listen"); close(listen_fd); exit(1);
     }
 
     printf("[Server] Listening on port %d for one player...\n", port);
 
     /* Accept exactly one client */
     struct sockaddr_in cli = {0};
     socklen_t cli_len = sizeof(cli);
     int client_fd = accept(listen_fd, (struct sockaddr*)&cli, &cli_len);
     if (client_fd < 0) {
         perror("accept"); close(listen_fd); exit(1);
     }
     printf("[Server] Player2 connected from %s:%d\n\n",
            inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));
 
     /* Send welcome and rules */
     const char *rules =
         "INFO:Welcome to Rock-Paper-Scissors-Lizard-Spock!\n"
         "INFO:Rules:\n"
         "INFO:  R beats S,L; P beats R,K;\n"
         "INFO:  S beats P,L; L beats P,K;\n"
         "INFO:  K beats R,S;\n"
         "INFO:Enter R/P/S/L/K to play, T=reset, Q=quit.\n\n";
     send(client_fd, rules, strlen(rules), 0);
 
     int scores[2] = {0, 0};
     int round = 1;
     char buf[BUF_SIZE];
 
     /* Game loop */
     while (1) {
         char info[BUF_SIZE];
 
         /* Notify client of round start and current score */
         snprintf(info, sizeof(info),
                  "INFO:Starting Round %d. Score P1:%d P2:%d\n\n",
                  round, scores[0], scores[1]);
         send(client_fd, info, strlen(info), 0);
 
         /* Prompt Player1 locally */
         printf("Enter move (R/P/S/L/K), T=reset, Q=quit: ");
         fflush(stdout);
 
         /* Wait for either local input or client input */
         fd_set rfds;
         FD_ZERO(&rfds);
         FD_SET(STDIN_FILENO, &rfds);
         FD_SET(client_fd,    &rfds);
         if (select(client_fd+1, &rfds, NULL, NULL, NULL) < 0) {
             perror("select");
             break;
         }
 
         /* 1) Handle local (Player1) input */
         if (FD_ISSET(STDIN_FILENO, &rfds)) {
             char c = getchar();
             while (getchar()!='\n');
             if (c == 'Q') {
                 send(client_fd, "QUIT", 4, 0);
                 printf("[Server] You quit.\n");
                 break;
             }
             if (c == 'T') {
                 scores[0] = scores[1] = 0;
                 send(client_fd, "RESET", 5, 0);
                 printf("[Server] Scores reset.\n\n");
                 continue;
             }
             Move m1 = char_to_move(c);
             if (m1 == MOVE_INVALID) {
                 printf("Invalid input. Try again.\n\n");
                 continue;
             }
 
             /* Inform client that Player1 has moved */
             snprintf(info, sizeof(info),
                      "INFO:Player1 has made a choice. Your turn.\n");
             send(client_fd, info, strlen(info), 0);
 
             /* 2) Receive Player2's command */
             int n = recv(client_fd, buf, BUF_SIZE-1, 0);
             if (n <= 0) {
                 printf("[Server] Player2 disconnected.\n");
                 break;
             }
             buf[n] = '\0';
             if (!strncmp(buf, "QUIT", 4)) {
                 printf("[Server] Player2 quit.\n");
                 break;
             }
             if (!strncmp(buf, "RESET", 5)) {
                 scores[0] = scores[1] = 0;
                 printf("[Server] Scores reset by Player2.\n\n");
                 continue;
             }
             Move m2 = MOVE_INVALID;
             if (!strncmp(buf, "MOVE:", 5)) {
                 m2 = char_to_move(buf[5]);
             }
             if (m2 == MOVE_INVALID) {
                 printf("[Server] Invalid move from Player2.\n\n");
                 continue;
             }
 
             /* Determine winner and update score */
             int w1 = beats(m1, m2);
             int w2 = beats(m2, m1);
             if (w1 && !w2) scores[0]++;
             else if (w2 && !w1) scores[1]++;
 
             /* Broadcast RESULT to client */
             snprintf(info, sizeof(info),
                      "RESULT:%c:%c:%d:%d",
                      move_to_char(m1),
                      move_to_char(m2),
                      scores[0],
                      scores[1]);
             send(client_fd, info, strlen(info), 0);
 
             /* Also print to server console */
             printf("[Server] %s\n\n", info);
         }
 
         /* 3) (Optional) handle unexpected client-initiated messages first */
         if (FD_ISSET(client_fd, &rfds)) {
             /* No action here since we drive the loop from local input */
         }
 
         round++;
     }
 
     close(client_fd);
     close(listen_fd);
     printf("[Server] Shutdown.\n");
     return 0;
 }
 