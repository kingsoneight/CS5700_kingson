/***********************************************************************
 * spock.c - Example: Rock, Paper, Scissors, Lizard, Spock over TCP
 *
 * Usage:
 *   Server: ./spock -s <port>
 *   Client: ./spock -c <hostname> <port>
 *
 * This is a minimal two-player demonstration.
 ***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#define BUF_SIZE 1024

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
void usage(const char *progname);
int start_server(int port);
int accept_client(int server_fd);
int connect_to_server(const char *host, int port);
Move char_to_move(char c);
const char *move_to_string(Move m);
int determine_winner(Move p1, Move p2);
void play_game(int sockfd, int is_server);

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    usage(argv[0]);
    return 1;
  }

  /* Determine mode (server/client) */
  if (strcmp(argv[1], "-s") == 0)
  {
    // Server Mode
    if (argc != 3)
    {
      usage(argv[0]);
      return 1;
    }
    int port = atoi(argv[2]);
    int server_fd = start_server(port);
    if (server_fd < 0)
    {
      fprintf(stderr, "Error starting server on port %d\n", port);
      return 1;
    }
    printf("Server listening on port %d...\n", port);

    int client_fd = accept_client(server_fd);
    if (client_fd < 0)
    {
      fprintf(stderr, "Error accepting client.\n");
      close(server_fd);
      return 1;
    }
    printf("Client connected. Starting game...\n");
    close(server_fd); // not accepting more connections in 2-player version

    // Server is Player 1
    play_game(client_fd, /*is_server=*/1);
    close(client_fd);
  }
  else if (strcmp(argv[1], "-c") == 0)
  {
    // Client Mode
    if (argc != 4)
    {
      usage(argv[0]);
      return 1;
    }
    const char *host = argv[2];
    int port = atoi(argv[3]);

    int sockfd = connect_to_server(host, port);
    if (sockfd < 0)
    {
      fprintf(stderr, "Error connecting to server.\n");
      return 1;
    }
    printf("Connected to server. Starting game...\n");
    // Client is Player 2
    play_game(sockfd, /*is_server=*/0);
    close(sockfd);
  }
  else
  {
    usage(argv[0]);
  }

  return 0;
}

void usage(const char *progname)
{
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "  Server: %s -s <port>\n", progname);
  fprintf(stderr, "  Client: %s -c <server_ip> <port>\n", progname);
}

/* Start a server socket on the given port. Returns server_fd or -1 on error. */
int start_server(int port)
{
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0)
  {
    perror("socket");
    return -1;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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

  if (listen(server_fd, 1) < 0)
  {
    perror("listen");
    close(server_fd);
    return -1;
  }

  return server_fd;
}

/* Accept a single client connection. Returns client_fd or -1 on error. */
int accept_client(int server_fd)
{
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
  if (client_fd < 0)
  {
    perror("accept");
    return -1;
  }
  return client_fd;
}

/* Connect to a server at host:port. Returns sockfd or -1 on error. */
int connect_to_server(const char *host, int port)
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

  // Convert host IP string to binary
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

/* Map character input to Move enum */
Move char_to_move(char c)
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

/* Get a string representation of a Move */
const char *move_to_string(Move m)
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

/* Determine winner between two moves:
 * Returns: 0 = tie, 1 = first wins, 2 = second wins */
int determine_winner(Move p1, Move p2)
{
  if (p1 == p2)
    return 0;

  /* We'll manually encode the winning combos */
  if ((p1 == MOVE_PAPER && p2 == MOVE_ROCK) ||
      (p1 == MOVE_PAPER && p2 == MOVE_SPOCK) ||
      (p1 == MOVE_SCISSORS && p2 == MOVE_PAPER) ||
      (p1 == MOVE_SCISSORS && p2 == MOVE_LIZARD) ||
      (p1 == MOVE_SPOCK && p2 == MOVE_SCISSORS) ||
      (p1 == MOVE_SPOCK && p2 == MOVE_ROCK) ||
      (p1 == MOVE_ROCK && p2 == MOVE_SCISSORS) ||
      (p1 == MOVE_ROCK && p2 == MOVE_LIZARD) ||
      (p1 == MOVE_LIZARD && p2 == MOVE_SPOCK) ||
      (p1 == MOVE_LIZARD && p2 == MOVE_PAPER))
  {
    return 1;
  }
  else
  {
    return 2;
  }
}

/* Main game loop for two-player mode. */
void play_game(int sockfd, int is_server)
{
  char buffer[BUF_SIZE];
  int score_p1 = 0;
  int score_p2 = 0;

  // Identify roles
  int player_id = is_server ? 1 : 2;
  printf("You are Player %d.\n", player_id);

  while (1)
  {
    /* Prompt user for command or move */
    printf("\nCommands:\n");
    printf(" [R]ock, [P]aper, [S]cissors, [L]izard, [K]Spock\n");
    printf(" [M] show score, [T] reset score, [Q] quit\n");
    printf("Enter your choice: ");
    fflush(stdout);

    if (!fgets(buffer, BUF_SIZE, stdin))
    {
      // Error or EOF
      printf("Error reading input. Quitting.\n");
      break;
    }

    // Remove newline
    char *nl = strchr(buffer, '\n');
    if (nl)
      *nl = '\0';
    if (strlen(buffer) < 1)
    {
      continue;
    }

    char cmd = buffer[0];
    if (cmd == 'Q' || cmd == 'q')
    {
      // Quit
      snprintf(buffer, BUF_SIZE, "QUIT");
      send(sockfd, buffer, strlen(buffer), 0);
      printf("You chose to quit. Exiting.\n");
      break;
    }
    else if (cmd == 'M' || cmd == 'm')
    {
      // Show score
      printf("[Score] Player1=%d, Player2=%d\n", score_p1, score_p2);
      continue;
    }
    else if (cmd == 'T' || cmd == 't')
    {
      // Reset scores
      snprintf(buffer, BUF_SIZE, "RESET");
      send(sockfd, buffer, strlen(buffer), 0);
      // We'll also set local to 0, and expect the other side does the same
      score_p1 = 0;
      score_p2 = 0;
      printf("[Scores reset]\n");
      continue;
    }
    else
    {
      // Possibly a move
      Move my_move = char_to_move(cmd);
      if (my_move == MOVE_INVALID)
      {
        printf("Invalid input. Try again.\n");
        continue;
      }
      // Send "MOVE:<char>" to other side
      snprintf(buffer, BUF_SIZE, "MOVE:%c", cmd);
      send(sockfd, buffer, strlen(buffer), 0);

      /* Now we wait for the other player's move. We also handle the
         possibility that the other player's message might arrive before
         we send ours, etc. */
      Move other_move = MOVE_INVALID;
      int got_move = 0;

      while (!got_move)
      {
        memset(buffer, 0, BUF_SIZE);
        int n = recv(sockfd, buffer, BUF_SIZE - 1, 0);
        if (n <= 0)
        {
          printf("Connection closed or error.\n");
          return;
        }

        buffer[n] = '\0';
        // Parse the message
        if (strncmp(buffer, "QUIT", 4) == 0)
        {
          printf("Other player quit. Exiting.\n");
          return;
        }
        else if (strncmp(buffer, "RESET", 5) == 0)
        {
          score_p1 = 0;
          score_p2 = 0;
          printf("[Scores reset by other player]\n");
          // keep waiting for the move (or we might re-ask user to re-enter)
          // but let's forcibly break so user can choose again
          break;
        }
        else if (strncmp(buffer, "MOVE:", 5) == 0)
        {
          char om = buffer[5];
          other_move = char_to_move(om);
          got_move = 1;
        }
        else
        {
          // Possibly ignore or handle other commands
        }
      }

      if (other_move == MOVE_INVALID)
      {
        // Something happened, let's continue
        continue;
      }

      // We have both moves now, let's compare
      int result = 0;
      if (is_server)
      {
        // p1 = me, p2 = other
        result = determine_winner(my_move, other_move);
      }
      else
      {
        // p1 = server, p2 = me
        // so if I'm Player 2, the "first" move in determine_winner is the server's move
        result = determine_winner(other_move, my_move);
      }

      /* The "result" variable is from the perspective:
       * 0 -> tie
       * 1 -> first wins
       * 2 -> second wins
       */

      // For printing, let's unify logic so it's from the local player's perspective:
      // We'll just do a straightforward approach:
      if (is_server)
      {
        // My move is p1, other is p2
        if (result == 0)
        {
          printf("You (%s) vs Player2 (%s) => Tie!\n",
                 move_to_string(my_move), move_to_string(other_move));
        }
        else if (result == 1)
        {
          score_p1++;
          printf("You (%s) vs Player2 (%s) => You win!\n",
                 move_to_string(my_move), move_to_string(other_move));
        }
        else
        {
          score_p2++;
          printf("You (%s) vs Player2 (%s) => Player2 wins!\n",
                 move_to_string(my_move), move_to_string(other_move));
        }
      }
      else
      {
        // My move is p2, other is p1
        if (result == 0)
        {
          printf("Player1 (%s) vs You (%s) => Tie!\n",
                 move_to_string(other_move), move_to_string(my_move));
        }
        else if (result == 1)
        {
          // Player1 wins
          score_p1++;
          printf("Player1 (%s) vs You (%s) => Player1 wins!\n",
                 move_to_string(other_move), move_to_string(my_move));
        }
        else
        {
          // Player2 (me) wins
          score_p2++;
          printf("Player1 (%s) vs You (%s) => You win!\n",
                 move_to_string(other_move), move_to_string(my_move));
        }
      }

      // Possibly display the updated score
      printf("[Score] Player1=%d, Player2=%d\n", score_p1, score_p2);
    }
  }

  // End of while loop
  printf("Game ended. Final scores: Player1=%d, Player2=%d\n", score_p1, score_p2);
}
