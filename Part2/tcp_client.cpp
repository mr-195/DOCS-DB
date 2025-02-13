// tcp_client.cpp

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "10.0.0.3" // Replace with the server's IP
#define PORT 12345
#define BUFFER_SIZE 2048

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    // Seed the random number generator
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Client: Socket creation error");
        exit(EXIT_FAILURE);
    }

    // Define server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IP address from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        std::cerr << "Client: Invalid address/Address not supported" << std::endl;
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Client: Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Generate random data between 512 bytes and 1KB
    int data_size = (std::rand() % 513) + 512; // Random size between 512 and 1024
    char *data = new char[data_size];

    // Fill data with random bytes
    for (int i = 0; i < data_size; ++i) {
        data[i] = static_cast<char>(std::rand() % 256);
    }

    // Send data to server
    send(sock, data, data_size, 0);

    // Receive response from server
    ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0'; // Null-terminate the received string
        std::cout << "Client: Received from server: " << buffer << " bytes" << std::endl;
    } else {
        perror("Client: Receive failed");
    }

    // Clean up
    delete[] data;
    close(sock);

    return 0;
}
