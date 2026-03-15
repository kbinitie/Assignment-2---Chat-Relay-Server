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
#define RECV_BUF_SIZE 1024
#define ACCUM_BUF_SIZE 2048

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
int send_all(int sockfd, const char *buf, int len);
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

int send_all(int sockfd, const char *buf, int len)
{
    int total_sent = 0;
    int n;

    while (total_sent < len) {
        n = send(sockfd, buf + total_sent, len - total_sent, 0);

        if (n <= 0) {
            return -1;
        }

        total_sent += n;
    }

    return 0;
}

void broadcast_message(int sender_sockfd, int sender_id, const char *msg)
{
    char outbuf[MAX_MSG_LEN + 32];
    int outlen;

    // required format is : <client_id>: <message>\n
    outlen = snprintf(outbuf, sizeof(outbuf), "%d: %s\n", sender_id, msg);

    if (outlen <= 0) {
        return;
    }

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active == 1 && clients[i].sockfd != sender_sockfd) {
            send_all(clients[i].sockfd, outbuf, outlen);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void *client_thread(void *arg)
{
    thread_arg_t *info = (thread_arg_t *)arg;
    int sockfd = info->sockfd;
    int client_id = info->client_id;

    char recvbuf[RECV_BUF_SIZE];
    char msgbuf[ACCUM_BUF_SIZE];
    int msg_len = 0;
    int discard_mode = 0;
    int n;

    printf("Client %d thread started\n", client_id);

    free(info);

    while (1) {
        n = recv(sockfd, recvbuf, sizeof(recvbuf), 0);

        if (n == 0) {
            printf("Client %d disconnected\n", client_id);
            break;
        }

        if (n < 0) {
            perror("recv");
            break;
        }

        for (int i = 0; i < n; i++) {
            char c = recvbuf[i];

            // if current message is already too long, ignore everything until a newline appears
            if (discard_mode == 1) {
                if (c == '\n') {
                    discard_mode = 0;
                    msg_len = 0;
                }
                continue;
            }

            // newline means one complete message is ready
            if (c == '\n') {
                msgbuf[msg_len] = '\0';

                if (msg_len > 0) {
                    broadcast_message(sockfd, client_id, msgbuf);
                }

                msg_len = 0;
            }
            else {
                if (msg_len < MAX_MSG_LEN) {
                    msgbuf[msg_len] = c;
                    msg_len++;
                }
                else {
                    // message exceeded 512 bytes before newline
                    discard_mode = 1;
                    msg_len = 0;
                }
            }
        }
    }

    remove_client(sockfd);
    close(sockfd);

    return NULL;
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

    // initialize client list
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].sockfd = -1;
        clients[i].client_id = -1;
        clients[i].active = 0;
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

    // now accepts clients and create one thread per client
    while (1) {
        int client_fd;
        int client_id;
        pthread_t tid;
        thread_arg_t *info;

        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        // to assign a unique client ID
        pthread_mutex_lock(&clients_mutex);
        client_id = next_client_id;
        next_client_id++;
        pthread_mutex_unlock(&clients_mutex);

        printf("Client %d connected\n", client_id);

        // need for allocating memory for thread arguments
        info = malloc(sizeof(thread_arg_t));
        if (info == NULL) {
            perror("malloc");
            close(client_fd);
            continue;
        }

        info->sockfd = client_fd;
        info->client_id = client_id;

        // create one thread for this client
        if (pthread_create(&tid, NULL, client_thread, info) != 0) {
            perror("pthread_create");
            close(client_fd);
            free(info);
            continue;
        }

        // add client to active client list
        add_client(client_fd, client_id, tid);

        // for thread to clean up its own resources when done
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}