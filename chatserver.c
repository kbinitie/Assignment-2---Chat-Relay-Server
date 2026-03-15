#include <stdio.h>        
#include <stdlib.h>       
#include <string.h>    
#include <unistd.h>     
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define MAX_CLIENTS 20
#define MAX_MSG_LEN 512

// client_t: store info about connected client
typedef struct {
    int sockfd;
    int client_id;
    int active; // 1 if slot in use otherwise 0
    pthread_t tid;
} client_t;

// thread_arg_t: to pass data into each client thread
typedef struct {
    int sockfd;
    int client_id;
} thread_arg_t;

// global client list storing all active clients
client_t clients[MAX_CLIENTS];

// need mutex; protect shared client list since multiple threads may access it at same time?
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

int next_client_id = 1; // increment when new client connects

// func prototypes
void *client_thread(void *arg);
void add_client(int sockfd, int client_id, pthread_t tid);
void remove_client(int sockfd);
void broadcast_message(int sender_sockfd, int sender_id, const char *msg);

void add_client(int sockfd, int client_id, pthread_t tid)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active == 0) {
            clients[i].sockfd = sockfd;
            clients[i].client_id = client_id;
            clients[i].tid = tid;
            clients[i].active = 1;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int sockfd)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active == 1 && clients[i].sockfd == sockfd) {
            clients[i].sockfd = -1;
            clients[i].client_id = -1;
            clients[i].active = 0;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

int main(int argc, char* argv[]){
    int server_fd;
    int port;
    struct sockaddr_in server_addr;

    // expected program usage: ./chatserver <port>
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return -1;
    }

    port = atoi(argv[1]);

    if (port <= 0) {
        printf("Invalid port\n");
        return -1;
    }

    // creating a TCP socket w/ IPv4
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }

    // used to clear server address structure before filling it 
    memset(&server_addr, 0, sizeof(server_addr));
    // ipv4 address family
    server_addr.sin_family = AF_INET;
    // accept connections on any local network interface
    server_addr.sin_addr.s_addr = INADDR_ANY;
    // convert the port number to network byte irder
    server_addr.sin_port = htons(port);

    // to bind the socket to the chosen port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return -1;
    }

    // mark socket as a listening socket
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    printf("Chat server listening on port %d\n", port);

    // just for now, accepting connections and immediately closing them
    while (1) {
        int client_fd;

        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        printf("Client connected\n");

        // close for now. TODO: add threads later
        close(client_fd);
    }

    close(server_fd);
    return 0;
}