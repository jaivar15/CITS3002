#include <stdio.h>
#include <string.h>   //strlen  
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   //close  
#include <arpa/inet.h>    //close  
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <stdbool.h>

#include "game.h"
#include "server.h"

/*
        CITS3002 COMPUTER NETWORKS PROEJCT 2019: BATTLE ROYLE 
        CONTRIBUTORS:
                    MITCHELL COX: 22233087
                    VARUN JAIN: 21963986
        DUE DATE: 23rd MAY 2019
*/


#define BUFFER_SIZE 1024                    
#define ROUND_TIMEOUT 20                    //each player has to make a MOVE within a 20 second time limit
#define LOBBY_TIMEOUT 30                    //server waits 30 seconds before a game can be started 

char *movebuf[MAXCLIENTS];
int *curRound;

/*
    Determines whether the player has made a move
*/

int recvdAllMoves() {
    for (int i = 0; i < getTotalPlayers(); ++i) {
        //return 0 if no move has been made
        if ((strcmp("", movebuf[i]) == 0) && checkLives(i) != -1) return 0;
    }
    //return 1 if player has made a movie
    return 1;
}

/*
    Resets the move buffers on the next round
*/

void resetMoveBuffers() {
    for (int i = 0; i < MAXCLIENTS; ++i) {

        //max message length 
        movebuf[i] = calloc(15, sizeof(char));
    }
    curRound = calloc(getTotalPlayers(), sizeof(int));
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int port = atoi(argv[1]);
    
    int server_fd = init_server(port);
    int start = 0;
    time_t startTime = time(NULL), roundStart;

    resetMoveBuffers();

    while (true) {
        char *buf = calloc(BUFFER_SIZE, sizeof(char));
        char *msg = malloc(14 * sizeof(char));
        int *clients;
        //Check sockets for accepting new connections
        checkSockets("accept"); 
        //start game when lobby ends with a minimum 4 players connected to the server
        if (time(NULL) - startTime > LOBBY_TIMEOUT && getTotalPlayers() > 3) { 
            start = 1;
            //kick clients and restart lobby if not enough players
        } else if (time(NULL) - startTime > LOBBY_TIMEOUT) { 
            //not enough players have joined, send CANCEL packet
            socket_send_all("CANCEL");
            resetMoveBuffers();
            shutdownGame();
            sleep(5);
            startTime = time(NULL);
            continue;
        }
        //countdown - last 5 seconds before server will accept a client, and before the game begins 
        if (start == 0) {
            if (time(NULL) - startTime > (LOBBY_TIMEOUT-5)) printf("Lobby ends in %d\n", (LOBBY_TIMEOUT+1) - (time(NULL) - startTime));
            sleep(1);
            continue;
        }

        while (recvdAllMoves() == 0) { //check if "INIT" packet recvd from all connected clients and place in buffer
            int pl = checkSockets(buf);
            if (strcmp(movebuf[pl], "") == 0 && pl > -1) movebuf[pl] = buf;
        }

        setupPointers();
        clients = getClientsFd();

        for (int i = 0; i < sizeof(clients); i++) { 
            if (clients[i] != 0) {
                //send WECLOME and START packets to all the clients connected to the server 
                init_welcome(movebuf[i], i, clients[i]);
            }
        }

        resetMoveBuffers();
        //start the game
        setupGame();
        roundStart = time(NULL);
        while (true) {

            buf = calloc(BUFFER_SIZE, sizeof(char)); // Clear our buffer so we don't accidentally send/print garbage

            int activity = checkSockets(buf); //check for activity on connected sockets

            if (activity == -1 && time(NULL) - roundStart < ROUND_TIMEOUT) continue;
            //prints out the packet sent by the client
            if (strcmp(buf, "") != 0) {
                movebuf[activity] = buf;
                printf("packet received = %s\n", buf);
            }
    
            if (activity == -2) {
                free(buf);
                free(msg);
                printf("Game has complete, server closing in...\n");
                for (int i = 5; i > 0; --i) {
                    printf("%d\n", i);
                    sleep(1);
                }
                //close server 
                exit(EXIT_SUCCESS);
            }
            //round timer up - player has not made a moved within the allocated 20s
            if (time(NULL) - roundStart > ROUND_TIMEOUT && recvdAllMoves() == 0) {
                for (int i = 0; i < getTotalPlayers(); ++i) {
                    if ((strcmp("", movebuf[i]) == 0) && checkLives(i) != -1) {
                        movebuf[i] = "TIMEOUT";
                    }
                }
            }
            
            if (recvdAllMoves()) {
                for (int i = 0; i < getTotalPlayers(); ++i) {
                    if (checkLives(i)==-1) continue;
                    //process MOVE made by the player 
                    curRound[i] = correct_message(movebuf[i], i);
                }
                //all players have beeen eliminated 
                if (pass_or_fail(curRound, msg) == 1){
                    free(buf);
                    free(msg);
                    //shutdown game
                    printf("Game has complete, server closing in...\n");
                    for (int i = 5; i > 0; --i) {
                        printf("%d\n", i);
                        sleep(1);
                    }
                    exit(EXIT_SUCCESS);
                }
                resetMoveBuffers();
                roundStart = time(NULL);
                nextRound();
            }
        }
    }
}

