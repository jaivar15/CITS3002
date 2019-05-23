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
#include "server.h"


#define BUFFER_SIZE 1024


enum moves {
    ODD, EVEN, DOUB, CON
};
const char *moves[] = {"ODD", "EVEN", "DOUB", "CON"};

//----------------------------------------------------------------GLOBAL VARIABLES------------------------------------------------------------

int *d1rolls;                       //dice 1 - stores the roll for die 1
int *d2rolls;                       //dice 2 - stores the roll for dice 2 
int *clientFds;                     //
int *clientIds;                     //client identification - 3 digit number 
int *livesLeft;                     //Remaining lives for player 
int totalPlayers;                   //total number of players - total number of clients connected to the server 

int lives = 3;                      //each player will have 3 lives 
int roundNo = -1;                   //round number
int players = 0;                    
int setup = 0;

//check lives of specified player
int checkLives(int player) {
    if (roundNo == -1) return 0;
    return livesLeft[player];
}

void setupPointers() {
    if (setup == 0) {
        setup++;
        totalPlayers = getTotalPlayers();
        clientIds = calloc(totalPlayers, sizeof(int));
        clientFds = calloc(totalPlayers, sizeof(int));
    }
}

//----------------------------------------------------------------START AND WECLOME-------------------------------------------------------------

/*
   Once all client connections have been established with the server, and game lobby has ended
        - if client connection accepted, each client will recieve their own WECLOME packet, with it's own unquie 3 digit identification number 
        - following that, alls client will recieve a START packet containing {START, number of players, number of lives}

*/

int init_welcome(char *buf, int player, int clientfd) {
    char *msg = calloc(14, sizeof(char));
    //check if "INIT" recvd == connected sockets, send WELCOME and START packet
    if (strcmp(buf, "INIT") == 0) {
        //randomly generates a 3 digit number - client's identification number 
        clientIds[players] = rand() % 899 + 100;
        //send WELCOME, unquie identification ID to call players connected to the server 
        sprintf(msg, "%s,%d", "WELCOME", clientIds[player]);
        send(clientfd, msg, strlen(msg), 0);
        clientFds[players] = clientfd;
        //increment the number of players 
        players++;
    } else {
        return -1;
    }

    if (players == getTotalPlayers()) { 
        sleep(3);
        msg[0] = '\0';
        //send all players START packet to all clients
        //START packet: START, number of players, number of lives 
        sprintf(msg, "%s,%d,%d", "START", players, lives);
        //send packet to all players 
        socket_send_all(msg);
        free(msg);
        return 1;
    }
    free(msg);
    return 0;
}

//------------------------------------------------------------START GAME AND SHUTDOWN------------------------------------------------------------

/*
    d1rolls stores 10 randomly generated single digit integors
    s2rolls stores 10 randomly generated single digit integors 
*/

void rollDice() {
    d1rolls = malloc(10 * sizeof(int));
    d2rolls = malloc(10 * sizeof(int));
    for (int i = 0; i < 10; ++i) {
        d1rolls[i] = rand() % 5 + 1;
        d2rolls[i] = rand() % 5 + 1;
    }
}

/*
    Initate the game -> roll the dice and give each player 3 lives 
*/ 

void setupGame() {
    rollDice();
    livesLeft = malloc(totalPlayers * sizeof(int));
    for (int j = 0; j < totalPlayers; ++j) {
        livesLeft[j] = lives;
    }
    roundNo = 0;
}


/*
    frees the memory, once the game has ended 
*/ 

void shutdownGame() {
    if (roundNo > -1) {
        free(d1rolls);
        free(d2rolls);
        free(livesLeft);
        free(clientFds);
        free(clientIds);
        roundNo = -1;
    }
    close_all_clients();
    setup = 0;
}


//---------------------------------------------------------------PARSE MESSAGE----------------------------------------------------------------

/*
    Server will recieve client's input respons
    Example parsed packet recieved: {777, MOV, EVEN}
    checks whether the corect packet has been recieved. 

*/

//parse received packet, cheaters via serverside id check, timeout setting
int *parseMessage(char *msg, int player) { 

    int *parsedInfo = malloc(2 * sizeof(int));

    //checks if player timeout 
    if (strcmp(msg,"TIMEOUT") == 0){ 
        parsedInfo[0] = -2;
        return parsedInfo;
    }
    //strtok divides the packet into tokens 
    char *splitmsg = strtok(msg, ",");

    //match id in msg with player id on server
    if (atoi(splitmsg) != clientIds[player]) parsedInfo[0] = -3; 

    splitmsg = strtok(NULL, ",");
    //checks if the MOV packet is not in the packet 
    if (strcmp(splitmsg, "MOV") != 0) parsedInfo[0] = -1; 

    splitmsg = strtok(NULL, ",");

    for (int i = 0; i < 4; ++i) {
        //checks players response matches with either: {"ODD", "EVEN", "DOUB", "CON"}
        if (strcmp(splitmsg, moves[i]) == 0) {
            parsedInfo[0] = i;
            //checks if player sends a CON, %d packet
            if (i == 3) {
                //returns incorrect packet, if packet is incomplete
                splitmsg = strtok(NULL, ","); 
                if (splitmsg == NULL) {
                    parsedInfo[0] = -1;
                } else {
                    //store %d in parsedInfo
                    parsedInfo[1] = atoi(splitmsg);
                }
            }
            break;
        }
        parsedInfo[0] = -1;
    }
    return parsedInfo;
}



//---------------------------------------------------------------CORRECT MESSAGE----------------------------------------------------------------

/*
    correct_message() recieve packet from parseMessage(). 
    Server will recieve one of the following client responses {EVEN, ODD, DOUB, CON %d}
    Compares players move against the current roll 
    return 1 if player move matches the current roll die 
    return 0, if incorrect 
    handles incorrect move packet, cheating, timeouts
*/


int correct_message(char *buf, int player) {
    //player move recieved
    int *parsedMsg = parseMessage(buf, player);
    int correct = -1;
    //incorrect packet sent by player, 
    if (parsedMsg[0] == -1){
        fprintf(stderr, "sent incorrect move packet\n");
        return 0;
    }

    //timeout - mark the move is incorrect for the player 
    if (parsedMsg[0] == -2) return 0;
    if (parsedMsg[0] == -3) {
        fprintf(stderr, "cheater detected, kicking from server\n");
        return -1;
    }
    switch (parsedMsg[0]) {
        case ODD:
            correct = ((((d1rolls[roundNo] + d2rolls[roundNo]) % 2) == 1) && ((d1rolls[roundNo] + d2rolls[roundNo]) > 5))? 1 : 0;
            break;
        case EVEN:
            if ((((d1rolls[roundNo] + d2rolls[roundNo]) % 2) == 0) && (d1rolls[roundNo] != d2rolls[roundNo])) {
                correct = 1;
            } else {
                correct = 0;
            }
            break;
        case DOUB:
            if (d1rolls[roundNo] == d2rolls[roundNo]) {
                correct = 1;
            } else {
                correct = 0;
            }
            break;
        case CON:
            if (d1rolls[roundNo] == parsedMsg[1] || d2rolls[roundNo] == parsedMsg[1]) {
                correct = 1;
            } else {
                correct = 0;
            }
            break;
    }
    free(parsedMsg);
    return correct;
}

//--------------------------------------------------------------WIN OR LOSE----------------------------------------------------------------

/*
    VICT Packet sent the the last standing player with the highest number of lives remaining
*/

int endCheck() {
    //only one player left in the game
    if (players == 1) {
        for (int i = 0; i < MAXCLIENTS; ++i) {
            //number of lives for player is greater than 0
            if (checkLives(i) > 0) {
                char *msg = malloc(14 * sizeof(int));
                //Player Identification, VICT packet sent to the winning player 
                sprintf(msg, "%d,%s", clientIds[i], "VICT");
                socket_send(clientFds[i], msg);
                free(msg);
                shutdownGame();
                return 1;
            }
        }
    //Edge CASE: all the players are eliminated -> shutdown game
    } else if (players == 0) {
        shutdownGame();
        return 1;
    } else return 0;
}

/*
    Handles eliminating players 
        - Sends Identification Id, ELIM packet to players with 0 lives remaining
        - returns 1 for game finishing, 0 for continuing
*/


int checkElim() {
    for (int i = 0; i < totalPlayers; ++i) {
        //number of lives for a play has reduced to 0
        if (livesLeft[i] == 0) {
            char *msg = malloc(14 * sizeof(int));
            //sends elimination message to players 
            sprintf(msg, "%d,%s", clientIds[i], "ELIM");
            socket_send(clientFds[i], msg);
            free(msg);
            //set number of lives to -1
            livesLeft[i] = -1;
            //reduce the number of players 
            players--;
            close_client(clientFds[i]);
            clientFds = getClientsFd();
            //all players have been eliminated 
            if (endCheck() == 1) return 1;
        }
    }
    return 0;
}

//--------------------------------------------------------------PASS OR FAIL----------------------------------------------------------------

/*
    Handles PASS/FAIL packet sending 
    Crosschecks players MOVE with the current roll dice
        -> Send PASS packet if player move correct
        -> Send FAIL packet if player move incorrect 
    returns 1 if the game is ending 
    returns 0 if the game is continuted to be played 

*/

int pass_or_fail(int *moves, char *msg) {

    //Incorrect packet sent by the player, send FAIL packet and decrement player life 
    for (int i = 0; i < totalPlayers; ++i) {
        if (moves[i] == -1 && checkLives(i) != -1){
            livesLeft[i] = -1;
        }
        if (moves[i] == 0 && checkLives(i) != -1){
            livesLeft[i]--;
        }
    }

    
    if (checkElim() == 1) {
        return 1;
    }

    for (int i = 0; i < totalPlayers; i++) {
        //Correct MOVE made by the player 
        if (moves[i] == 1){
            msg[0] = '\0';
            //send PASS packet 
            sprintf(msg, "%d,%s", clientIds[i], "PASS");
            socket_send(clientFds[i], msg);
            //incorrect MOVE made by player
        } else if (moves[i] == 0 && checkLives(i) > 0) {
            msg[0] = '\0';
            //send FAIL packet
            sprintf(msg, "%d,%s", clientIds[i], "FAIL");
            socket_send(clientFds[i], msg);
        }
    }
    return 0;
}


void setLives(int player, int lives) {
    livesLeft[player] = lives;
}

//returns current round number 
int getRoundNo() {
    return roundNo;
}

//next round, if 10 rounds have been completed, roll the dice again
void nextRound(){
        roundNo++;
        if (roundNo % 10 == 0) rollDice();
}

//returns player unquie 3 digit identification number 
int getPlayerID(int i){
    return clientIds[i];
}

//decrement the number of players if someone disconnects or loses. 
void decrementPlayers(){
    players--;
}
