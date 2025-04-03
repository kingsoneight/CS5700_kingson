/******************************************************************************
 * spock_client.c
 *
 * A multi-player client that connects to the spock_server, sends user commands/
 * moves, and interprets the server's results.
 *
 * Usage:
 *   ./spock_client <server_ip> <port>
 * Example:
 *   ./spock_client 127.0.0.1 5555
 *
 * The server orchestrates the rounds. The client only needs to:
 *   1) Prompt the user for either a move (R/P/S/L/K) or a command (M=score local,
 *      T=reset request, Q=quit).
 *   2) Send it to the server (MOVE:<char>, RESET, QUIT).
 *   3) Wait for the server's broadcast result or commands.
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define BUF_SIZE 1024

static int connect_to_server(const char *host, int port);

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
    exit(1);
  }
  const char *server_ip = argv[1];
  int port = atoi(argv[2]);

  int sockfd = connect_to_server(server_ip, port);
  if (sockfd < 0)
  {
    fprintf(stderr, "Could not connect to server.\n");
    return 1;
  }
  printf("Connected to server at %s:%d.\n", server_ip, port);

  char buffer[BUF_SIZE];
  int local_score = 0; // you can track your own or all scores if you like

  fd_set read_fds;
  int max_fd = sockfd > fileno(stdin) ? sockfd : fileno(stdin);

  while (1)
  {
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);        // watch server socket
    FD_SET(fileno(stdin), &read_fds); // watch stdin

    int ret = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
    if (ret < 0)
    {
      perror("select");
      break;
    }

    /* Check if there's data from the server */
    if (FD_ISSET(sockfd, &read_fds))
    {
      memset(buffer, 0, BUF_SIZE);
      int n = recv(sockfd, buffer, BUF_SIZE - 1, 0);
      if (n <= 0)
      {
        printf("Server closed the connection.\n");
        break;
      }
      buffer[n] = '\0';

      // Parse server message
      if (strncmp(buffer, "QUIT", 4) == 0)
      {
        printf("Server signaled QUIT. Exiting...\n");
        break;
      }
      else if (strncmp(buffer, "RESET", 5) == 0)
      {
        printf("Server reset the scores.\n");
        local_score = 0;
      }
      else if (strncmp(buffer, "RESULT:", 7) == 0)
      {
        // Format: RESULT:<winnerIndex>:move0,move1,...:score0,score1,...
        // We'll just print it out nicely.
        printf("[Game Update] %s\n", buffer + 7);

        // If you want to parse your new local_score from the trailing data, do so.
        // ...
      }
      else
      {
        // Other server data
        printf("[Server] %s\n", buffer);
      }
    }

    /* Check if there's user input on stdin */
    if (FD_ISSET(fileno(stdin), &read_fds))
    {
      memset(buffer, 0, BUF_SIZE);
      if (!fgets(buffer, BUF_SIZE, stdin))
      {
        printf("Error reading input or EOF.\n");
        break;
      }
      // Strip newline
      char *nl = strchr(buffer, '\n');
      if (nl)
        *nl = '\0';

      if (strlen(buffer) == 0)
      {
        continue;
      }

      char cmd = buffer[0];
      if (cmd == 'Q' || cmd == 'q')
      {
        // Quit
        send(sockfd, "QUIT", 4, 0);
        printf("You chose to quit.\n");
        break;
      }
      else if (cmd == 'T' || cmd == 't')
      {
        // Reset request
        send(sockfd, "RESET", 5, 0);
      }
      else if (cmd == 'M' || cmd == 'm')
      {
        // Locally show our own score, or just a message
        printf("[Local Score] = %d (this is purely local tracking)\n", local_score);
      }
      else
      {
        // Maybe a move: R/P/S/L/K
        // We'll just send MOVE:<char>
        // (We rely on the server to do the logic & keep the official score.)
        char msg[BUF_SIZE];
        snprintf(msg, BUF_SIZE, "MOVE:%c", cmd);
        send(sockfd, msg, strlen(msg), 0);
      }
    }
  } // end while(1)

  close(sockfd);
  return 0;
}

static int connect_to_server(const char *host, int port)
{
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    perror("socket");
    return -1;
  }

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0)
  {
    perror("inet_pton");
    close(sockfd);
    return -1;
  }

  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    perror("connect");
    close(sockfd);
    return -1;
  }
  return sockfd;
}
