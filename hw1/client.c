/**
 ** client.c  -  a server program that uses the socket interface to tcp
 **
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include "client.h"
#include <errno.h>

extern char *inet_ntoa(struct in_addr);

#define NAMESIZE 255
#define BUFSIZE 81

// ANSI color codes for prettier output
#define COLOR_RESET "\x1b[0m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_BLUE "\x1b[34m"
#define COLOR_CYAN "\x1b[36m"

// Helper function to clear the screen
void clear_screen()
{
  printf("\033[2J\033[H");
}

// Helper function to print colored messages
void print_message(const char *prefix, const char *message, const char *color)
{
  printf("%s%s: %s%s", color, prefix, message, COLOR_RESET);
}

void send_reply(int client_fd, const char *message)
{
  int len = strlen(message);
  int n = send(client_fd, message, len, 0);
  if (n != len)
  {
    char err_msg[BUFSIZE];
    fprintf(stderr, "Error: %s\n", strerror(errno));
    snprintf(err_msg, BUFSIZE, "ERROR: %s", strerror(errno));
    send(client_fd, err_msg, strlen(err_msg), 0);
    exit(1);
  }
}

void client(int server_number, char *server_node)
{
  int length;
  int n, len;
  short fd;
  struct sockaddr_in address;
  struct hostent *node_ptr;
  char local_node[NAMESIZE];
  char buffer[BUFSIZE];
  int my_turn = 1;
  int chat_over = 0;

  /*  get the internet name of the local host node on which we are running  */
  if (gethostname(local_node, NAMESIZE) < 0)
  {
    perror("client gethostname");
    exit(1);
  }
  fprintf(stderr, "client running on node %s\n", local_node);

  /*  get the name of the remote host node on which we hope to find server  */
  if (server_node == NULL)
    server_node = local_node;
  fprintf(stderr, "client about to connect to server at port number %d on node %s\n",
          server_number, server_node);

  /*  get structure for remote host node on which server resides  */
  if ((node_ptr = gethostbyname(server_node)) == NULL)
  {
    perror("client gethostbyname");
    exit(1);
  }

  /*  set up Internet address structure for the server  */
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  memcpy(&address.sin_addr, node_ptr->h_addr, node_ptr->h_length);
  address.sin_port = htons(server_number);

  fprintf(stderr, "client full name of server node %s, internet address %s\n",
          node_ptr->h_name, inet_ntoa(address.sin_addr));

  /*  open an Internet tcp socket  */
  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("client socket");
    exit(1);
  }

  /*  connect this socket to the server's Internet address  */
  if (connect(fd, (struct sockaddr *)&address, sizeof(address)) < 0)
  {
    perror("client connect");
    exit(1);
  }

  /*  now find out what local port number was assigned to this client  */
  len = sizeof(address);
  if (getsockname(fd, (struct sockaddr *)&address, &length) < 0)
  {
    perror("client getsockname");
    exit(1);
  }

  /*  we are now successfully connected to a remote server  */
  fprintf(stderr, "client at internet address %s, port %d\n",
          inet_ntoa(address.sin_addr), ntohs(address.sin_port));

  /*  transmit data from standard input to server  */
  while (!chat_over)
  {
    if (my_turn)
    {
      /* ---- Write Mode ---- */
      print_message("[INPUT]", " Your turn to speak (Type 'x' to end turn, 'xx' to quit):\n", COLOR_GREEN);
      printf("> ");

      while (fgets(buffer, BUFSIZE, stdin) != NULL)
      {

        /* Check for control signals */
        if (strcmp(buffer, "xx\n") == 0)
        {
          /* Send termination signal and mark chat over */
          if (send(fd, buffer, len, 0) != len)
          {
            perror("client send");
            exit(1);
          }
          chat_over = 1;
          break; /* Exit writing loop */
        }

        if (strcmp(buffer, "x\n") == 0)
        {
          /* Send end-of-turn signal, then switch to read mode */
          if (send(fd, buffer, len, 0) != len)
          {
            perror("client send");
            exit(1);
          }
          break; /* End of our writing turn */
        }

        /* For a normal message, send it */
        if (send(fd, buffer, len, 0) != len)
        {
          perror("client send");
          exit(1);
        }
      } /* end inner writing loop */

      /* If chat_over was signaled, break out of the outer loop */
      if (chat_over)
        break;

      /* Switch to read mode */
      my_turn = 0;
    }
    else
    {
      /* ---- Read Mode ---- */
      print_message("[WAITING]", " Waiting for server response...\n", COLOR_BLUE);

      while ((n = recv(fd, buffer, BUFSIZE - 1, 0)) > 0)
      {
        buffer[n] = '\0'; /* Null-terminate the received string */

        /* Check for control signals from the server */
        if (strcmp(buffer, "xx\n") == 0)
        {
          chat_over = 1;
          break;
        }
        if (strcmp(buffer, "x\n") == 0)
        {
          /* Server has finished its turn; switch back to write mode */
          break;
        }

        /* Otherwise, print the received message */
        print_message("[SERVER]", buffer, COLOR_CYAN);
        printf("\n");
      }

      if (n < 0)
      {
        print_message("[ERROR]", " Failed to receive data from server.\n", COLOR_BLUE);
        perror("client recv");
        exit(1);
      }

      if (chat_over)
      {
        print_message("[EXIT]", " Chat session ended. Disconnecting...\n", COLOR_BLUE);
        break;
      }

      /* Switch back to write mode */
      my_turn = 1;
    }
  }
}