#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include "server.h"
#include "game.h"

#define BUFFER_SIZE 1024

int *clients, server_fd, totalPlayers = 0;

//--------------------------------------------------------ESTABLISH SERVER CLIENT CONNECTION------------------------------------------------------------

/*
	createSocket() creates an endpoint for communication
	Return: returns a socket descriptor respresenting the end point 
		- returns a nonnegative socket descriptor if sucessful
		- returns a negative socket descriptor if unsuccessful
*/

int createSocket() {

    int server_fd;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        fprintf(stderr, "Could not create socket\n");
        exit(EXIT_FAILURE);
    }
    printf("Connection Established\n");
    return server_fd;
}

/*
	Once the server creates the socket 
		- bind(): assigns the socket an address
		- listen(): listens for incoming connection from the client. 
		  Specifies the maximum number of connecton that can be queued for the server socket. 
	Return: returns socket descriptor
*/

int init_server(int port) {
    int opt_val, errno;

    server_fd = createSocket();

    struct sockaddr_in server;

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    opt_val = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val) == -1) {
        fprintf(stderr,"Could not set sockopt\n");
        exit(EXIT_FAILURE);
    }

    errno = bind(server_fd, (struct sockaddr *) &server, sizeof(server));
    if (errno < 0) {
        fprintf(stderr, "Could not bind socket\n");
        exit(EXIT_FAILURE);
    }

    errno = listen(server_fd, MAXCLIENTS);
    if (errno < 0) {
        fprintf(stderr, "Could not listen on socket\n");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on %d\n", port);

    clients = calloc(MAXCLIENTS, sizeof(int));

    return server_fd;
}


//---------------------------------------------------------------SEND MESSAGE------------------------------------------------------------

/*
	socket_sends(): sends a message from the server to the connected client. 
			- buffer: sends the message stored in the buffer, 
			- send() returns the number of bytes sent.
	Return: the message that is sent to the client. Points to the buffer containing the message to send. 
*/

int socket_send(int client_fd, char *buffer) {
    //int players; //number of clients joined the server
    int errno;
    //char* buffer;
    printf("sent %s to %d\n", buffer, client_fd);
    fflush(stdout);
    errno = send(client_fd, buffer, strlen(buffer), 0);
    if (errno < 0) {
        fprintf(stderr, "Could not recvieve transmission\n");
        exit(EXIT_FAILURE);
    }
    return errno;
}

/*
    Send all connected clients the same message
*/

int socket_send_all(char *buffer) {
    for (int i = 0; i < totalPlayers; ++i) {
        errno = socket_send(clients[i], buffer);
    }

    return errno;
}


//-----------------------------------------------------------ACCEPT CONNECTION------------------------------------------------------------

/*
	accept(): establishes a connection with the specific client.
			  Server accepts a conection request from the client.
			  Extracts of the first connection on the queue for pending connection
	Return: returns the non-negative file descriptor of the accepted socket, if succeessful.
*/

int socket_accept(int server_fd) {
    int client_fd;
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);

    client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);

    if (client_fd < 0) {
        fprintf(stderr, "Could not establish new connection\n");
    }
    //game in progress, REJECT any new incoming connections 
    if (getRoundNo() != -1 || totalPlayers == MAXCLIENTS) {
        socket_send(client_fd, "REJECT");
        close(client_fd);
        return -1;
    }

    printf("Connection accepted: %s %d %d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port), client_fd);

    for (int i = 0; i < MAXCLIENTS; i++) {
        if (clients[i] == 0) {
            clients[i] = client_fd;
            printf("Adding to list of players as player %d\n", i);
            totalPlayers++;
            break;
        }
    }

    return 1;
}


//--------------------------------------------------------------MULTI-CLIENT------------------------------------------------------------

/*
    Establish Multiclient Connections and checks socket descriptor for activity. 
  ` 
*/


int checkSockets(char *buf) {

    fd_set readfds; 
    int max_sd = server_fd, sd, selectno;
    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);

    for (int i = 0; i < MAXCLIENTS; i++) {
        sd = clients[i];

        if (sd > 0) FD_SET(sd, &readfds);

        if (sd > max_sd) max_sd = sd;
    }

    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;

    selectno = select(max_sd + 1, &readfds, NULL, NULL, &timeout);

    if (selectno == 0) return -1;

    if ((selectno < 0)) {
        fprintf(stderr, "Select error, %d", errno);
        return -1;
    }

    if (FD_ISSET(server_fd, &readfds)) { //Check for incoming connections
        socket_accept(server_fd);
        return -1;
    }

    if (strcmp(buf, "accept") == 0) return -1; //Dont receive data if still accepting connections

    for (int i = 0; i < MAXCLIENTS; i++) {
        sd = clients[i];
        if (FD_ISSET(sd, &readfds)) {
            int recvd = recv(sd, buf, BUFFER_SIZE, 0);
            if (recvd < 0) {
                fprintf(stderr, "Error reading from socket\n");
                sleep(2);
                i--;
                continue;
            } else if (recvd == 0) {
                printf("Player:%d disconnected\n", getPlayerID(i)); //Somebody disconnected
                setLives(i, -1); //Close the socket and mark as player as eliminated
                close(sd);
                clients[i] = 0;
                decrementPlayers();
                if (endCheck() == 1) return -2;
            } else {
                buf[recvd] = '\0';
                return i; //player number message received is from
            }
        }
    }
}

//returns the total number of players currently connected to the game
int getTotalPlayers() {
    return totalPlayers;
}

//retuns clients socket descriptor 
int *getClientsFd() {
    return clients;
}

//----------------------------------------------------------------CLOSE CLIENTS------------------------------------------------------------

void close_client(int client_fd) {
    close(client_fd);
    for (int i = 0; i < totalPlayers; ++i) {
        if (clients[i] == client_fd) clients[i] = 0;
    }
}

void close_all_clients() {
    for (int i = 0; i < totalPlayers; ++i) {
        if (clients[i] != 0) {
            close_client(clients[i]);
            clients[i] = 0;
        }
    }
    totalPlayers = 0;
}





