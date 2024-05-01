//daemon.c
#include "daemon_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>

#define SOCKET_PATH "/tmp/daemon_socket"
#define STORAGE_PATH "/tmp/daemon_storage"

void handle_client(int client_socket);

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

void listen_for_connections(int server_socket) {
    while (1) {
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket == -1) {
            perror("Failed to accept connection");
            continue;
        }

        // Handle the client
        handle_client(client_socket);
    }
}


void handle_client(int client_socket) {
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

    buffer[num_bytes] = '\0';

    char *command = strtok(buffer, ":");
    char *data = strtok(NULL, ":");

    if (command == NULL || data == NULL) {
        printf("Received invalid command from client: %s\n", buffer);
        close(client_socket);
        return;
    }


    if (strcmp(command, "COMMAND") == 0) {
        printf("Received COMMAND from client: %s\n", data);
    } else if (strcmp(command, "SEND_NEW_BLOCK") == 0) {
            printf("Received SEND_NEW_BLOCK from client: %s\n", data);

            // Append the block data to the storage file
            FILE *storage_file = fopen(STORAGE_PATH, "a");
            if (storage_file == NULL) {
                perror("Failed to open storage file");

            }

            // Assuming data is a string representation of the block
            fprintf(storage_file, "%s\n", data);
            fclose(storage_file);

            const char *response = "Block received and stored successfully";
            if (write(client_socket, response, strlen(response)) == -1) {
                perror("write"); }
    } else {
        // Unknown command
        printf("Received unknown command from client: %s\n", command);
        const char *response = "Unknown command.";
        if (write(client_socket, response, strlen(response)) == -1) {
            perror("write");
        }
    }

    close(client_socket);
}



int main() {
    initialize_daemon();

    // Create the socket file
    int server_socket = create_socket_file();
    if (server_socket == -1) {
        perror("Failed to create socket file");
        exit(EXIT_FAILURE);
    }



    // Main loop to accept and handle connections
    while (1) {

                // Listen for incoming connections
        if (listen(server_socket, 5) == -1) {
            perror("Failed to listen on socket");
            close(server_socket);
            exit(EXIT_FAILURE);
        }

        printf("Daemon listening for incoming connections...\n");
            int client_socket = accept(server_socket, NULL, NULL);
            if (client_socket == -1) {
                perror("Failed to accept connection");
                continue;
            }

            // Handle the client
            handle_client(client_socket);
        }

    // Cleanup the socket when the program exits
    cleanup_socket();
    return 0;
}