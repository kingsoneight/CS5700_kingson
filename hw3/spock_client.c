/******************************************************************************
 * spock_client.c
 *
 * A multi-player client that connects to the spock_server. The client:
 *   1) Receives "RESULT:" broadcasts after each round (and "RESET"/"QUIT").
 *   2) Only prints a prompt for a new command when needed (i.e., after round ends
 *      or a reset), to avoid spamming the user while waiting for other players.
 *   3) Allows user to type:
 *       - R/P/S/L/K -> Move
 *       - T -> "RESET"
 *       - Q -> "QUIT"
 *       - M -> local score display (if you track it)
 *
 * Usage example:
 *   ./spock_client 127.0.0.1 5555
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define BUF_SIZE 1024

static void usage(const char *prog);
static int connect_to_server(const char *host, int port);

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    usage(argv[0]);
    exit(1);
  }

  const char *server_ip = argv[1];
  int port = atoi(argv[2]);

  int sockfd = connect_to_server(server_ip, port);
  if (sockfd < 0)
  {
    fprintf(stderr, "Error: could not connect to server.\n");
    return 1;
  }
  printf("[Client] Connected to server at %s:%d\n", server_ip, port);

  /* If you want to keep track of your own local score, create a variable here. */
  int local_score = 0; // purely optional local tracking
  char buffer[BUF_SIZE];

  fd_set read_fds;
  int max_fd = (sockfd > fileno(stdin)) ? sockfd : fileno(stdin);

  /* We'll use a 'prompt_needed' flag so we only show the prompt
   * when the user is allowed to pick a new command (i.e., at round start).
   */
  int prompt_needed = 1;

  while (1)
  {
    // If we need to prompt the user for input, do it once:
    if (prompt_needed)
    {
      printf("\n--------------------------------------------------\n");
      printf("Enter command:\n"
             "  R: Rock      P: Paper\n"
             "  S: Scissors  L: Lizard\n"
             "  K: Spock     T: Reset\n"
             "  Q: Quit      M: Show local score\n"
             "Your command: ");
      fflush(stdout);
    }

    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);
    FD_SET(fileno(stdin), &read_fds);

    int ret = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
    if (ret < 0)
    {
      perror("select");
      break;
    }

    /* 1) Check if server sent something */
    if (FD_ISSET(sockfd, &read_fds))
    {
      memset(buffer, 0, BUF_SIZE);
      int n = recv(sockfd, buffer, BUF_SIZE - 1, 0);
      if (n <= 0)
      {
        printf("[Client] Server closed connection.\n");
        break;
      }
      buffer[n] = '\0';

      // parse server message
      if (strncmp(buffer, "QUIT", 4) == 0)
      {
        printf("[Client] Server signaled QUIT. Exiting...\n");
        break;
      }
      else if (strncmp(buffer, "RESET", 5) == 0)
      {
        printf("[Client] Scores have been reset (server broadcast).\n");
        // This means we start a new round => prompt again
        prompt_needed = 1;
        // If you keep local_score, set it to 0 or do nothing
        local_score = 0;
      }
      else if (strncmp(buffer, "RESULT:", 7) == 0)
      {
        // This indicates the round ended. Let's show the outcome.
        printf("[Client] Round Result => %s\n", buffer + 7);
        // Possibly parse out your new local score from the message, if you want.
        // For now, just mention the round ended.
        // Start a new round => re-prompt
        prompt_needed = 1;
      }
      else
      {
        // some other server message
        printf("[Client] Server says: %s\n", buffer);
      }
    }

    /* 2) Check if the user typed something */
    if (FD_ISSET(fileno(stdin), &read_fds))
    {
      memset(buffer, 0, BUF_SIZE);
      if (!fgets(buffer, BUF_SIZE, stdin))
      {
        printf("[Client] Error reading input or EOF.\n");
        break;
      }
      // strip newline
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
        send(sockfd, "QUIT", 4, 0);
        printf("[Client] You chose to quit.\n");
        break;
      }
      else if (cmd == 'T' || cmd == 't')
      {
        // user requests RESET
        send(sockfd, "RESET", 5, 0);
        // We won't re-prompt until server confirms with a "RESET" msg
        prompt_needed = 0;
      }
      else if (cmd == 'M' || cmd == 'm')
      {
        // show local score
        printf("[Client] Local Score = %d (not official)\n", local_score);
        // do not clear prompt_needed, user can still pick a move
      }
      else
      {
        // probably a move: R/P/S/L/K
        // We'll send as "MOVE:R"
        char msg[BUF_SIZE];
        snprintf(msg, BUF_SIZE, "MOVE:%c", cmd);
        send(sockfd, msg, strlen(msg), 0);

        // They have effectively made their move => we won't prompt again
        // until the server finishes the round (RESULT) or we get a RESET, etc.
        prompt_needed = 0;
      }
    }
  } // end while(1)

  close(sockfd);
  return 0;
}

static void usage(const char *prog)
{
  fprintf(stderr, "Usage: %s <server_ip> <port>\n", prog);
  fprintf(stderr, "Example: %s 127.0.0.1 5555\n", prog);
}

/* connect_to_server: create a TCP connection to server_ip:port. */
static int connect_to_server(const char *host, int port)
{
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    perror("socket");
    return -1;
  }

  struct sockaddr_in srv;
  memset(&srv, 0, sizeof(srv));
  srv.sin_family = AF_INET;
  srv.sin_port = htons(port);

  if (inet_pton(AF_INET, host, &srv.sin_addr) <= 0)
  {
    perror("inet_pton");
    close(sockfd);
    return -1;
  }

  if (connect(sockfd, (struct sockaddr *)&srv, sizeof(srv)) < 0)
  {
    perror("connect");
    close(sockfd);
    return -1;
  }
  return sockfd;
}
