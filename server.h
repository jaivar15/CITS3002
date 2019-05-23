#define MAXCLIENTS 20 //Max number of clients, can change to support more

int createSocket();
int init_server(int port);
int socket_accept(int server_fd);
int socket_send(int client_fd, char* buffer);
int socket_send_all(char* buffer);
int checkSockets(char* buf);
int getTotalPlayers();
int *getClientsFd();
void close_client(int client_fd);
void close_all_clients();
