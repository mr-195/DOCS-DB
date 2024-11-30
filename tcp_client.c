// tcp_client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "10.0.1.2"  // IP address of NS3
#define SERVER_PORT 12346
#define BUFFER_SIZE 1024

void generate_random_data(char *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buffer[i] = rand() % 256;  // Random byte
    }
}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid server IP address");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Generate random data between 512 and 1024 bytes
    size_t data_size = (rand() % 513) + 512;  // Random size between 512 and 1024
    generate_random_data(buffer, data_size);

    // Send data to server
    send(sock, buffer, data_size, 0);

    // Receive the server's response (length of received data)
    ssize_t recv_size = recv(sock, buffer, sizeof(buffer), 0);
    if (recv_size > 0) {
        printf("Received from server: %s bytes\n", buffer);
    }

    close(sock);
    return 0;
}
