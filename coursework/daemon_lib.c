#include "daemon_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>

#define SOCKET_PATH "/tmp/daemon_socket"

// Global variable declaration
int sockfd = -1; // Initialize to -1 to indicate that the socket is not open

// Function to initialize the daemon
void initialize_daemon() {
    printf("Daemon starting\n");
    // Open the socket here if needed
}

// Function to cleanup the socket
void cleanup_socket() {
    if (sockfd != -1) {
        close(sockfd);
        sockfd = -1; // Reset to -1 to indicate that the socket is closed
    }
    unlink(SOCKET_PATH);
}

// Function to send a command to the daemon
int send_command(const char *command) {
    struct sockaddr_un addr;

    // Check if the socket is already open
    if (sockfd == -1) {
        // Create socket
        if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            return -1;
        }

        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

        // Connect to daemon
        if (connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
            perror("connect");
            close(sockfd);
            sockfd = -1; // Reset to -1 to indicate that the socket is closed
            return -1;
        }
    }

    // Send command to daemon
    if (write(sockfd, command, strlen(command)) == -1) {
        perror("write");
        close(sockfd);
        sockfd = -1; // Reset to -1 to indicate that the socket is closed
        return -1;
    }

    return 0;
}

// Function to send a new block of data to the daemon
uint8_t sendNewBlock(char *ID, uint8_t *secret, uint32_t data_length, void *data) {
    // Construct command
    char command[1024];
    sprintf(command, "SEND_NEW_BLOCK:%s:%d:", ID, data_length);

    // Calculate total length of data to send
    size_t total_length = strlen(command) + data_length;

    // Allocate memory for combined command and data
    char *combined_data = (char *)malloc(total_length);
    if (combined_data == NULL) {
        perror("malloc");
        return 0; // Failed to allocate memory
    }

    // Copy command into combined_data
    strcpy(combined_data, command);

    // Copy data into combined_data
    memcpy(combined_data + strlen(command), data, data_length);

    // Send command and data to the daemon
    if (send_command(combined_data) == -1) {
        free(combined_data);
        return 0; // Failed to send command
    }

    // Receive and interpret response from daemon
    char response[1024];
    ssize_t num_bytes = read(sockfd, response, sizeof(response) - 1);
    if (num_bytes == -1) {
        perror("read");
        free(combined_data);
        return 0; // Error reading response
    } else if (num_bytes == 0) {
        printf("No response received from daemon\n");
        free(combined_data);
        return 0; // No response received
    } else {
        response[num_bytes] = '\0'; // Null-terminate the received message
        printf("Response from daemon: %s\n", response);
        free(combined_data);
        return 1; // Success
    }
}

// Function to get a data block from the daemon
uint8_t getBlock(char *ID, uint8_t *secret, uint32_t buffer_size, void *buffer) {
    // Construct command
    char command[1024];
    sprintf(command, "GET_BLOCK:%s:%d:", ID, buffer_size);

    // Send command to daemon
    if (send_command(command) == -1) {
        return 0; // Failure
    }

    // Receive the data from the daemon
    ssize_t num_bytes = read(sockfd, buffer, buffer_size);
    if (num_bytes == -1) {
        perror("read");
        return 0; // Error reading data
    } else if (num_bytes == 0) {
        printf("No data received from daemon\n");
        return 0; // No data received
    } else {
        printf("Data received from daemon: %s\n", (char *)buffer);
        return 1; // Success
    }
}


