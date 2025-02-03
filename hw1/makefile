#
# Makefile for speak (client) and speakd (server)
#

# Compiler and Flags
CC = gcc
CFLAGS = -g -Wall

# Header and Source Files
HDR = client.h server.h
SRC = client.c server.c speak.c speakd.c
OBJ = speak.o speakd.o server.o client.o

# Targets
all: speak speakd

# Compile Client (speak)
speak: speak.o client.o
	$(CC) $(CFLAGS) speak.o client.o -o speak

# Compile Server (speakd)
speakd: speakd.o server.o
	$(CC) $(CFLAGS) speakd.o server.o -o speakd

# Compile Client Main File
speak.o: speak.c client.h
	$(CC) $(CFLAGS) -c speak.c

# Compile Client Core
client.o: client.c client.h
	$(CC) $(CFLAGS) -c client.c

# Compile Server Main File
speakd.o: speakd.c server.h
	$(CC) $(CFLAGS) -c speakd.c

# Compile Server Core
server.o: server.c server.h
	$(CC) $(CFLAGS) -c server.c

# Clean Up
clean:
	/bin/rm -f *.o speak speakd
