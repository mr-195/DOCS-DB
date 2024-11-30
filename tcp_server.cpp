// tcp_server.cpp

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 12345
#define BUFFER_SIZE 2048

int main() {
    int server_fd, client_sock;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    char buffer[BUFFER_SIZE];

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Server: Socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options (optional)
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt))) {
        perror("Server: Setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Define server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Bind to all interfaces
    address.sin_port = htons(PORT);

    // Bind the socket to the address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Server: Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening for clients
    if (listen(server_fd, 3) < 0) {
        perror("Server: Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    std::cout << "Server listening on port " << PORT << "..." << std::endl;

    while (true) {
        // Accept an incoming connection
        if ((client_sock = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("Server: Accept failed");
            continue;
        }

        std::cout << "Server: Connection established" << std::endl;

        // Receive data from the client
        ssize_t bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received < 0) {
            perror("Server: Receive failed");
            close(client_sock);
            continue;
        }

        // Send back the length of the data received
        std::string response = std::to_string(bytes_received);
        send(client_sock, response.c_str(), response.length(), 0);

        std::cout << "Server: Received " << bytes_received << " bytes, sent response." << std::endl;

        // Close the client socket
        close(client_sock);
    }

    // Close the server socket (unreachable code in this loop)
    close(server_fd);

    return 0;
}
