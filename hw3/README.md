Spock Network Game
==================

Overview:
---------
This project implements a network game in C using TCP sockets that is based on
the game "Rock, Paper, Scissors, Lizard, Spock" (inspired by The Big Bang Theory).
It supports multi-player play (up to 5 players) where the server acts as a referee.
In each round, one or more players can win if their move is dominant against the
others, thereby awarding multiple winners per round.

Features:
---------
- Multi-player support: The server accepts connections from multiple clients.
- Multiple winners: All players who choose a dominant move win the round.
- Commands available on the client:
    R: Rock
    P: Paper
    S: Scissors
    L: Lizard
    K: Spock
    T: Reset game scores (resets the current round without quitting)
    Q: Quit the game (terminates the connection)
    M: Show the local score (client-only tracking)
- The client prompt appears only when a new move is expected (i.e., at the start
  of a new round or after a reset).

File Structure:
---------------
- spock_server.c : Server application (referee mode; accepts client connections,
                   handles rounds, computes winners, and broadcasts results).
- spock_client.c : Client application (connects to server, sends moves/commands,
                   and displays game updates).
- Makefile       : For compiling the project.
- README.txt     : This file.

Compilation:
------------
1. Ensure you have gcc installed.
2. Run the following command in the project directory:
   
   $ make

   This will compile both the server and client executables.

Usage:
------
1. Start the server first. For example, to start the server on TCP port 5555
   expecting 3 players, run:

   $ ./spock_server 5555 3

2. Start each client in separate terminal windows (or on different machines).
   For example, to connect from a client to the server running on localhost:

   $ ./spock_client 127.0.0.1 5555

Gameplay:
---------
- When prompted, the client displays a menu with the following commands:

      R: Rock      P: Paper
      S: Scissors  L: Lizard
      K: Spock     T: Reset
      Q: Quit      M: Show local score

- The client prompt is shown once at the start of a round or after a reset.
- After entering a move, the client waits for the server to collect moves from
  all players and then displays the round result.
- If any player enters "T", the game scores are reset, and a new round begins.
- If any player enters "Q", the game ends for all players.

Cleaning Up:
------------
To remove the compiled executables, run:

   $ make clean

Notes:
------
- This implementation is a basic demonstration and may require further error
  handling or enhancements for production use.
- The server acts solely as a referee in this version and does not participate
  as a player.
- Enjoy the game!

