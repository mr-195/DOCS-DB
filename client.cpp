// client.cpp

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 4096
#define PORT 6379
#define SERVER_IP "127.0.0.1" // Change to server's IP if needed

// Function to serialize command to RESP format
std::string serialize_command(const std::vector<std::string>& cmd_parts) {
    std::string resp = "*" + std::to_string(cmd_parts.size()) + "\r\n";
    for (const auto& part : cmd_parts) {
        resp += "$" + std::to_string(part.size()) + "\r\n" + part + "\r\n";
    }
    return resp;
}

// Function to parse RESP response from server
void parse_response(int sock_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        std::cout << "(nil)" << std::endl;
        return;
    }
    buffer[bytes_received] = '\0';
    std::string response(buffer);
    char type = response[0];
    if (type == '+') {
        // Simple string
        std::cout << response.substr(1, response.find("\r\n") - 1) << std::endl;
    } else if (type == '$') {
        // Bulk string
        if (response.substr(1, 2) == "-1") {
            std::cout << "(nil)" << std::endl;
        } else {
            size_t pos = response.find("\r\n");
            int length = std::stoi(response.substr(1, pos - 1));
            std::string data = response.substr(pos + 2, length);
            std::cout << data << std::endl;
        }
    } else if (type == '-') {
        // Error
        std::cout << "(error) " << response.substr(1, response.find("\r\n") - 1) << std::endl;
    } else {
        std::cout << "Unknown response" << std::endl;
    }
}

// Function to split input line into command parts
std::vector<std::string> parse_input(const std::string& line) {
    std::vector<std::string> parts;
    std::istringstream iss(line);
    std::string part;
    while (iss >> part) {
        parts.push_back(part);
    }
    return parts;
}

int main() {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("Socket creation failed");
        return -1;
    }

    // Connect to server
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid server IP address" << std::endl;
        close(sock_fd);
        return -1;
    }

    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        close(sock_fd);
        return -1;
    }

    std::string line;
    std::cout << "Connected to DOCS DB server." << std::endl;
    std::cout << "> ";
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            std::cout << "> ";
            continue;
        }
        std::vector<std::string> cmd_parts = parse_input(line);
        std::string resp_cmd = serialize_command(cmd_parts);
        send(sock_fd, resp_cmd.c_str(), resp_cmd.size(), 0);
        parse_response(sock_fd);
        std::cout << "> ";
    }

    close(sock_fd);
    return 0;
}
