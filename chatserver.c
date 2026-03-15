#include <stdio.h>        
#include <stdlib.h>       
#include <string.h>    
#include <unistd.h>     
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

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