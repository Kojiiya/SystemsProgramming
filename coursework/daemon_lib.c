//daemon_lib.c
#include "daemon_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#define FIFO_PATH "/tmp/daemon_fifo"

int fifo_fd = -1;

void initialize_daemon() {
    printf("Daemon starting\n");
    fifo_fd = -1;
}

uint8_t getBlock(char *ID, uint8_t *secret, uint32_t buffer_size, void *buffer) {
    // Construct command
    char command[1024];
    sprintf(command, "GET_BLOCK:%s:", ID);

    // Append secret to command
    char secret_str[33];
    for (int i = 0; i < 16; i++) {
        sprintf(secret_str + (i * 2), "%02x", secret[i]);
    }
    secret_str[32] = '\0';
    strcat(command, secret_str);
    strcat(command, ":");

    // Append buffer_size to command
    char buffer_size_str[11];
    sprintf(buffer_size_str, "%u", buffer_size);
    strcat(command, buffer_size_str);
    strcat(command, ":");

    // Send command to daemon
    if (send_command(command, &fifo_fd) == -1) {
        return -1; // Command send failure
    }

    // Read the response from the daemon
    char response[1024];
    ssize_t num_bytes = read(fifo_fd, response, sizeof(response) - 1);
    if (num_bytes == -1) {
        perror("read");
        close(fifo_fd);
        return -2; // Read failure
    } else if (num_bytes == 0) {
        printf("No response received from daemon\n");
        close(fifo_fd);
        return -3; // No response received
    }
    response[num_bytes] = '\0'; // Null-terminate the received response

    // Check if the response is the expected data
    if (strncmp(response, "DATA:", 5) == 0) {
        // Extract the data length from the response
        char data_len_str[64];
        strncpy(data_len_str, response + 5, sizeof(data_len_str) - 1);
        data_len_str[sizeof(data_len_str) - 1] = '\0'; // Ensure null-termination

        // Tokenize the copied string to extract the data length
        char *token = strtok(data_len_str, ":");
        uint32_t data_bytes = atoi(token); // Convert the data length string to an integer

        // Calculate the number of bytes to read, accounting for the length of the data length string and the delimiter
        size_t bytes_to_read = data_bytes - (strlen(token) + 1); // +1 for the delimiter

        // Read the actual data into the buffer
        ssize_t bytes_read = read(fifo_fd, buffer, bytes_to_read);
        if (bytes_read == -1) {
            perror("read");
            close(fifo_fd);
            return -5; // Read failure
        } else if (bytes_read == 0) {
            printf("No data received from daemon\n");
            close(fifo_fd);
            return -6; // No data received
        } else {
            printf("Received data from daemon: %.*s\n", (int)bytes_read, (char *)buffer);
            return bytes_read; // Success, return the actual data length received
        }
    } else {
        // The daemon sent an unexpected response
        printf("Unexpected response from daemon: %s\n", response);
        close(fifo_fd);
        return -7; // Unexpected response
    }
}


int send_command(const char *command, int *fifo_fd_ptr) {
    if (access(FIFO_PATH, F_OK) == -1) {
        if (mkfifo(FIFO_PATH, 0666) == -1) {
            perror("mkfifo");
            return -1;
        }
    }

    // Open FIFO for reading and writing
    if (*fifo_fd_ptr == -1) {
        *fifo_fd_ptr = open(FIFO_PATH, O_RDWR);
        if (*fifo_fd_ptr == -1) {
            perror("open");
            return -1;
        }
    }

    ssize_t bytes_written = write(*fifo_fd_ptr, command, strlen(command));
    if (bytes_written == -1) {
        perror("write");
        close(*fifo_fd_ptr);
        *fifo_fd_ptr = -1;
        return -1;
    } else if (bytes_written != strlen(command)) {
        fprintf(stderr, "Incomplete write: %ld bytes written, %zu bytes expected\n", bytes_written, strlen(command));
        close(*fifo_fd_ptr);
        *fifo_fd_ptr = -1;
        return -1;
    }

    printf("Command sent to daemon: %s\n", command);

    return 0; // Success
}

uint8_t sendNewBlock(char *ID, uint8_t *secret, uint32_t data_length, void *data) {
    // Construct command
    char command[1024];
    int command_len = snprintf(command, sizeof(command), "SEND_NEW_BLOCK:%s:", ID);

    // Append secret to command
    char secret_str[33];
    for (int i = 0; i < 16; i++) {
        sprintf(secret_str + (i * 2), "%02x", secret[i]);
    }
    secret_str[32] = '\0';
    command_len += snprintf(command + command_len, sizeof(command) - command_len, "%s:", secret_str);

    // Append data_length to command
    command_len += snprintf(command + command_len, sizeof(command) - command_len, "%u:", data_length);

    // Ensure there's enough space for the data and null-termination
    if (data_length > sizeof(command) - command_len - 1) {
        fprintf(stderr, "Error: Data length exceeds command buffer size\n");
        return 1; // Command send failure
    }

    // Directly copy data into command, ensuring it's null-terminated
    memcpy(command + command_len, data, data_length);
    command[command_len + data_length] = '\0'; // Null-terminate the command

    // Send command to daemon
    if (send_command(command, &fifo_fd) == -1) {
        return 1; // Command send failure
    }

    return 0; // Success
}


void print_secret(uint8_t secret[], size_t size){
    for (int i = 0; i < size; i++) {
        printf("%02x", secret[i]);
    } 
    printf("\n");
}
