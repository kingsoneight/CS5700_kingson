/******************************************************************************
 * spock_client.c  (PATCHED FOR INTEROPERABILITY)
 *
 * A two‑player client that connects to any “Rock, Paper, Scissors, Lizard,
 * Spock” server listening on port 5131.
 *
 * Usage:
 *   gcc -Wall -Wextra -O2 spock_client.c -o spock_client
 *   ./spock_client <server_ip> [port]
 ******************************************************************************/

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <ctype.h>  // for toupper()

 #include <arpa/inet.h>
 #include <sys/socket.h>
 #include <sys/select.h>   // PATCH: already implicit, but keep it explicit
 
 #define BUF_SIZE 1024
 
 /* ─────────── Function Prototypes ─────────── */
 void  usage(const char *prog);
 int   connect_to_server(const char *server_ip, int port);
 
 int main(int argc, char *argv[])
 {
     if (argc < 2 || argc > 3) {
         usage(argv[0]);
         return 1;
     }
 
     const char *server_ip = argv[1];
     int port = 5131;
     if (argc == 3) {
         port = atoi(argv[2]);
         if (port <= 0) port = 5131;
     }
 
     int sockfd = connect_to_server(server_ip, port);
     if (sockfd < 0) {
         fprintf(stderr, "Error: could not connect to server %s:%d\n",
                 server_ip, port);
         return 1;
     }
     printf("[Client] Connected to %s:%d\n", server_ip, port);
 
     char  buffer[BUF_SIZE];
     int   prompt_needed = 1;   /* 1 = we should ask the user for a move */
 
     while (1) {
         fd_set readfds;
         FD_ZERO(&readfds);
         FD_SET(sockfd, &readfds);
 
         /* Only poll the keyboard when we are ready for the next move. */
         if (prompt_needed) {
             printf("Enter move (R/P/S/L/K), T=reset, Q=quit: ");
             fflush(stdout);
             FD_SET(STDIN_FILENO, &readfds);
         }
 
         if (select(sockfd + 1, &readfds, NULL, NULL, NULL) < 0) {
             perror("select");
             break;
         }
 
         /* ───────────── Handle LOCAL keyboard input ───────────── */
         if (prompt_needed && FD_ISSET(STDIN_FILENO, &readfds)) {
             char cmd = getchar();
             while (getchar() != '\n');            /* flush rest of line */
 
             char msg[BUF_SIZE];
             if (cmd == 'Q' || cmd == 'q') {
                 send(sockfd, "QUIT", 4, 0);
                 break;
             } else if (cmd == 'T' || cmd == 't') {
                 send(sockfd, "RESET", 5, 0);
             } else if (strchr("RrPpSsLlKk", cmd)) {
                 snprintf(msg, sizeof(msg), "MOVE:%c", (char)toupper(cmd));
                 send(sockfd, msg, strlen(msg), 0);
                 prompt_needed = 0;                /* wait for server */
             } else {
                 printf("Invalid command. Use R/P/S/L/K, T, or Q.\n");
             }
         }
 
         /* ───────────── Handle SERVER messages ───────────── */
         if (FD_ISSET(sockfd, &readfds)) {
             int n = recv(sockfd, buffer, BUF_SIZE - 1, 0);
             if (n <= 0) {
                 printf("[Client] Server disconnected.\n");
                 break;
             }
             buffer[n] = '\0';
 
             if (strncmp(buffer, "QUIT", 4) == 0) {
                 printf("[Client] Server signaled QUIT. Exiting...\n");
                 break;
 
             } else if (strncmp(buffer, "RESET", 5) == 0) {
                 printf("[Client] Game reset by server.\n");
                 prompt_needed = 1;
 
             } else if (strncmp(buffer, "RESULT:", 7) == 0) {
                 printf("[Client] Round Result => %s\n", buffer + 7);
                 prompt_needed = 1;
 
             }
             /* ───── PATCH: Accept INFO / PROMPT from other implementations ───── */
             else if (strncmp(buffer, "INFO:", 5) == 0) {
                 /* Always show the informational line. */
                 printf("[Server‑INFO] %s\n", buffer + 5);
 
                 /* If the info contains "Your turn" OR you simply want to
                  * treat any INFO as a cue that it is now our turn,
                  * enable keyboard input.  This makes us compatible with
                  * classmates who only send INFO instead of RESULT.       */
                 prompt_needed = 1;
             }
             else if (strncmp(buffer, "PROMPT", 6) == 0) {   // OPTIONAL extra
                 prompt_needed = 1;
             }
             /* ───── END PATCH ───── */
             else {
                 printf("[Client] Unknown message: %s\n", buffer);
             }
         }
     }
 
     close(sockfd);
     printf("[Client] Connection closed.\n");
     return 0;
 }
 
 /* ─────────── Utility functions ─────────── */
 
 void usage(const char *prog)
 {
     fprintf(stderr,
         "Usage: %s <server_ip> [port]\n"
         "  server_ip: IP or hostname of the server\n"
         "  port     : optional TCP port (default 5131)\n",
         prog);
 }
 
 int connect_to_server(const char *server_ip, int port)
 {
     int sockfd;
     struct sockaddr_in serv_addr;
 
     if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
         perror("socket() failed");
         return -1;
     }
     memset(&serv_addr, 0, sizeof(serv_addr));
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_port   = htons(port);
 
     if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
         perror("inet_pton() failed");
         close(sockfd);
         return -1;
     }
     if (connect(sockfd,
                 (struct sockaddr *)&serv_addr,
                 sizeof(serv_addr)) < 0) {
         perror("connect() failed");
         close(sockfd);
         return -1;
     }
     return sockfd;
 }
 