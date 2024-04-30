#include "daemon_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>

#define SOCKET_PATH "/tmp/daemon_socket"

int create_socket_file() {
    // Remove any existing socket file
    unlink(SOCKET_PATH);

    // Create a new socket file
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return -1;
    }

    // Set up the address structure
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    // Bind the socket to the address
    if (bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
        perror("bind");
        close(fd);
        return -1;
    }

    return fd;
}

void handle_client(int client_socket) {
    // Receive command or data from client
    char buffer[1024];
    ssize_t num_bytes = read(client_socket, buffer, sizeof(buffer) - 1);

    if (num_bytes == -1) {
        perror("read");
        close(client_socket);
        return;
    } else if (num_bytes == 0) {
        printf("Client disconnected.\n");
        close(client_socket);
        return;
    }

    // Null-terminate the received message
    buffer[num_bytes] = '\0';

    // Find the colon delimiter
    char *colon_pos = strchr(buffer, ':');
    if (colon_pos == NULL) {
        // Invalid command format
        printf("Received invalid command from client: %s\n", buffer);
        close(client_socket);
        return;
    }

    // Extract the command and data
    *colon_pos = '\0';  // Null-terminate the command
    char *command = buffer;
    char *data = colon_pos + 1;  // Data starts after the colon

    // Process the received command
    if (strcmp(command, "COMMAND") == 0) {
        // Handle command
        printf("Received command from client: %s\n", data);

        // Print the received data
        printf("Received data: %s\n", data);

        // Respond to the client
        const char *response = "Command received successfully.";
        if (write(client_socket, response, strlen(response)) == -1) {
            perror("write");
        }
    } else if (strcmp(command, "TEST_COMMAND") == 0) {
        // Handle test command
        printf("Received test command from client: %s\n", data);

        // Print the received data
        printf("Received data: %s\n", data);

        // Respond to the client
        const char *response = "Test command received successfully.";
        if (write(client_socket, response, strlen(response)) == -1) {
            perror("write");
        }
    } else if (strcmp(command, "SEND_NEW_BLOCK") == 0) {
        // Handle new block command
        printf("Received new block command from client: %s\n", data);

        // Process the block data (extract ID, secret, data length, and data itself)

        // Respond to the client indicating success or failure
        const char *response = "New block command received successfully.";
        if (write(client_socket, response, strlen(response)) == -1) {
            perror("write");
        }
    } else {
        // Unknown command
        printf("Received unknown command from client: %s\n", command);

        // Respond with an error message
        const char *response = "Unknown command.";
        if (write(client_socket, response, strlen(response)) == -1) {
            perror("write");
        }
    }

    // Close the client socket
    close(client_socket);
}

int main() {
    // Create the socket file
    int server_socket = create_socket_file();
    if (server_socket == -1) {
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) == -1) {
        perror("listen");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Daemon listening for incoming connections...\n");


    // Accept incoming connections and handle them
    while (1) {
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket == -1) {
            perror("accept");
            continue;
        }

        // Handle the client
        handle_client(client_socket);
        
    }

    // Close the server socket (this code is unreachable)
    close(server_socket);
    return 0;
}
