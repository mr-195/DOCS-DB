// tcp_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 12346
#define BUFFER_SIZE 1024

void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    ssize_t recv_size;

    // Receive data from client
    recv_size = recv(client_sock, buffer, sizeof(buffer), 0);
    if (recv_size > 0) {
        // Send back the length of the data
        char response[64];
        snprintf(response, sizeof(response), "%ld", recv_size);
        send(client_sock, response, strlen(response), 0);
    }
    close(client_sock);
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Create socket
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to the address
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_sock, 10) == -1) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    // Accept client connections
    while ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len)) != -1) {
        printf("Client connected: %s\n", inet_ntoa(client_addr.sin_addr));
        handle_client(client_sock);
    }

    if (client_sock == -1) {
        perror("Accept failed");
    }

    close(server_sock);
    return 0;
}
