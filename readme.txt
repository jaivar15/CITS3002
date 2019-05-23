CITS3002 COMPUTER NETWORKS PROJECT 2019: RNG BATTLE ROYLE 
CONTRIBUTORS:
            MITCHELL COX: 22233087
            VARUN JAIN: 21963986
DUE DATE: 23rd MAY 2019

Test on MAC Computers 
How to Compile:
		Command Line: make
		Command Line: ./server [port number]

Once server link has been established, clients have approximately 30s before the game proceeds. A minimum of 4 clients are required otherwise CANCEL packet sent to each client connected to the server. Re-establish connections, 30s time resets. 

Once VICT packet sent or all clients have been eliminated -> server will shutdown. 

Each round lasts for approximately 20 seconds. If players fail to send a move, they lose a life.

