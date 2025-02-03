/**
 ** server.c  -  a server program that uses the socket interface to tcp
 **
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include "server.h"
#include <errno.h>

extern char *inet_ntoa(struct in_addr);

#define NAMESIZE 255
#define BUFSIZE 81
#define listening_depth 2

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

void display_help()
{
	printf("\n%sAvailable commands:%s\n", COLOR_CYAN, COLOR_RESET);
	printf("  x    - End your turn\n");
	printf("  xx   - End chat session\n");
	printf("  clear- Clear screen\n");
	printf("  help - Show this help message\n\n");
}

void server(int server_number)
{
	int c, i;
	int n, len;
	short fd, client_fd;
	struct sockaddr_in address, client;
	struct hostent *node_ptr;
	char local_node[NAMESIZE];
	char buffer[BUFSIZE + 1];
	char reply[BUFSIZE + 1];
	int chat_over = 0;
	int server_turn = 0;

	/*  get the internet name of the local host node on which we are running  */
	if (gethostname(local_node, NAMESIZE) < 0)
	{
		perror("server gethostname");
		exit(1);
	}
	fprintf(stderr, "server running on node %s\n", local_node);

	/*  get structure for local host node on which server resides  */
	if ((node_ptr = gethostbyname(local_node)) == NULL)
	{
		perror("server gethostbyname");
		exit(1);
	}

	/*  set up Internet address structure for the server  */
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	memcpy(&address.sin_addr, node_ptr->h_addr, node_ptr->h_length);
	address.sin_port = htons(server_number);

	fprintf(stderr, "server full name of server node %s, internet address %s\n",
					node_ptr->h_name, inet_ntoa(address.sin_addr));

	/*  open an Internet tcp socket  */
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("server socket");
		exit(1);
	}

	/*  bind this socket to the server's Internet address  */
	if (bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		perror("server bind");
		exit(1);
	}

	/*  now find out what local port number was assigned to this server  */
	len = sizeof(address);
	if (getsockname(fd, (struct sockaddr *)&address, &len) < 0)
	{
		perror("server getsockname");
		exit(1);
	}

	/*  we are now successfully established as a server  */
	fprintf(stderr, "server at internet address %s, port %d\n",
					inet_ntoa(address.sin_addr), ntohs(address.sin_port));

	/*  start listening for connect requests from clients  */
	if (listen(fd, listening_depth) < 0)
	{
		perror("server listen");
		exit(1);
	}

	/*  now accept a client connection (we'll block until one arrives)  */
	len = sizeof(client);
	if ((client_fd = accept(fd, (struct sockaddr *)&client, &len)) < 0)
	{
		perror("server accept");
		exit(1);
	}

	clear_screen();
	print_message("SYSTEM", "Chat server started! Type 'help' for commands.\n", COLOR_CYAN);
	/*  we are now successfully connected to a remote client  */
	fprintf(stderr, "server connected to client at Internet address %s, port %d\n",
					inet_ntoa(client.sin_addr), ntohs(client.sin_port));

	while (!chat_over)
	{
		if (server_turn == 0)
		{
			// ----- Client's Turn: Receive messages in a loop -----
			print_message("SYSTEM", "Waiting for client's messages...\n", COLOR_CYAN);
			while (1)
			{
				n = recv(client_fd, buffer, BUFSIZE - 1, 0);

				if (n < 0)
				{
					// System call error: print and send error message back to client.
					fprintf(stderr, "Error in file %s: %s\n", __FILE__, strerror(errno));
					snprintf(reply, BUFSIZE, "ERROR in file %s: %s", __FILE__, strerror(errno));
					send_reply(client_fd, reply);
					exit(1);
				}
				buffer[n] = '\0'; // Ensure null-termination

				// Check for control signals.
				if (strcmp(buffer, "xx\n") == 0)
				{
					print_message("SYSTEM", "Client ended the chat.\n", COLOR_CYAN);
					send_reply(client_fd, "server received: xx\n");
					chat_over = 1;
					break; // Exit the inner loop; chat will terminate.
				}
				if (strcmp(buffer, "x\n") == 0)
				{
					print_message("SYSTEM", "Client ended its turn.\n", COLOR_CYAN);
					send_reply(client_fd, "server received: x\n");
					break; // End of client's turn; exit inner loop to switch turns.
				}

				// For a normal message, print it and reply.
				print_message("Client", buffer, COLOR_BLUE);
				snprintf(reply, BUFSIZE, "Server received: %s", buffer);
				send_reply(client_fd, reply);
			} // End inner while for client's turn

			if (chat_over)

				break;

			// Switch to server's turn.
			server_turn = 1;
		}
		else
		{ // server_turn == 1
			// ----- Server's Turn: Send messages until "x" or "xx" is entered -----
			print_message("SYSTEM", "Your turn to speak (enter 'x' to end your turn, 'xx' to quit):\n", COLOR_GREEN);
			printf(COLOR_GREEN "> " COLOR_RESET);
			while (1)
			{
				if (fgets(buffer, BUFSIZE, stdin) == NULL)
				{
					fprintf(stderr, "Error reading input\n");
					continue;
				}
				int len = strlen(buffer);

				if (strcmp(buffer, "help\n") == 0)
				{
					display_help();
					printf(COLOR_GREEN "> " COLOR_RESET);
					continue;
				}

				if (strcmp(buffer, "clear\n") == 0)
				{
					clear_screen();
					printf(COLOR_GREEN "> " COLOR_RESET);
					continue;
				}

				// Check for server control signals.
				if (strcmp(buffer, "xx\n") == 0)
				{
					send_reply(client_fd, buffer);
					chat_over = 1;
					break; // End server's turn and chat.
				}
				if (strcmp(buffer, "x\n") == 0)
				{
					send_reply(client_fd, buffer);
					break; // End server's turn.
				}

				// For a normal message, send it.
				send_reply(client_fd, buffer);
				print_message("You", buffer, COLOR_GREEN);
				// The protocol does not require waiting for an acknowledgement here.
				// (Every message from the client already receives a reply.)
			} // End inner while for server's turn

			if (chat_over)
				break;

			// Switch back to client's turn.
			server_turn = 0;
		}
	}

	if (n < 0)
		perror("server read");

	/*  close the connection to the client  */
	if (close(client_fd) < 0)
	{
		perror("server close connection to client");
		exit(1);
	}

	/*  close the "listening post" socket by which server made connections  */
	if (close(fd) < 0)
	{
		perror("server close");
		exit(1);
	}
}
