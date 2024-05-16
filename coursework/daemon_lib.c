#include "daemon_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#define FIFO_READ_PATH "/tmp/daemon_fifo_read"
#define FIFO_WRITE_PATH "/tmp/daemon_fifo_write"

// Global file descriptors for FIFOs
int fifo_write_fd = -1;
int fifo_read_fd = -1;

/**
 * Retrieves block of data from daemon using the provided ID/secret
 * @param ID Unique id for block.
 * @param secret Secret for authentication.
 * @param buffer_size Size of buffer to hold  returned data.
 * @param buffer Pointer to the buffer that will receive the data.
 * @return Returns 0 on success, negative values indicate errors.
 */
uint8_t getBlock(char *ID, uint8_t *secret, uint32_t buffer_size, void *buffer) {
    char command[1024];
    sprintf(command, "GET_BLOCK:%s:", ID); // Construct command with ID

    char secret_str[33];
    for (int i = 0; i < 16; i++) {
        sprintf(secret_str + (i * 2), "%02x", secret[i]); // Convert secret to hex string
    }
    secret_str[32] = '\0';
    strcat(command, secret_str); // Append secret to cmd

    fifo_write_fd = open(FIFO_WRITE_PATH, O_WRONLY); 
    if (fifo_write_fd == -1) {
        perror("open");
        return -1;
    }

    if (send_command(command) == -1) { 
        return -1;
    }

    close(fifo_write_fd);

    fifo_read_fd = open(FIFO_READ_PATH, O_RDONLY); 
    if (fifo_read_fd == -1) {
        perror("open");
        return -1;
    }

    ssize_t num_bytes = 0;
    char response[1024] = {0}; 
    while ((num_bytes = read(fifo_read_fd, response, sizeof(response) - 1)) > 0) {
        response[num_bytes] = '\0'; 
        printf("Received response from daemon: %s\n", response);

        if (strncmp(response, "DATA:", 5) == 0) {
            printf("Data\n");
            break; // Expected data received
        } else if (strncmp(response, "ACCESS_DENIED", 14) == 0) {
            printf("Access Denied\n");
            close(fifo_read_fd);
            break;
        } else {
            printf("Unexpected response from daemon: %s\n", response);
            close(fifo_read_fd);
            fifo_read_fd = -1;
            return -7; 
        }
    }

    if (num_bytes == -1) {
        perror("read");
        close(fifo_read_fd);
        return -2; 
    } else if (num_bytes == 0) {
        printf("No response received from daemon\n");
        close(fifo_read_fd);
        return -3; 
    }

    close(fifo_read_fd);
    return -1;
}

/**
 * Sends command to daemon via the write FIFO.
 * @param command The command string to send.
 * @return Returns 0 on success, -1 on failure.
 */
int send_command(const char *command) {
    int fifo_write_fd = open(FIFO_WRITE_PATH, O_WRONLY); 
    if (fifo_write_fd == -1) {
        perror("open");
        return -1;
    }

    size_t command_len = strlen(command) + 1; 
    ssize_t bytes_written = write(fifo_write_fd, command, command_len); 
    if (bytes_written == -1) {
        perror("write");
        close(fifo_write_fd);
        return -1;
    } else if (bytes_written!= command_len) {
        fprintf(stderr, "Incomplete write: %ld bytes written, %zu bytes expected\n", bytes_written, command_len);
        close(fifo_write_fd);
        return -1;
    }

    printf("Command sent to daemon: %s\n", command);
    close(fifo_write_fd);

    return 0; // Success
}

/**
 * Sends a new block of data to the daemon.
 * @param ID Unique id for block.
 * @param secret Secret for authentication.
 * @param data_length Length of data being sent.
 * @param data Pointer to data to send.
 * @return Returns 0 on success, 1 on failure.
 */
uint8_t sendNewBlock(char *ID, uint8_t *secret, uint32_t data_length, void *data) {
    char command[1024];
    int command_len = snprintf(command, sizeof(command), "SEND_NEW_BLOCK:%s:", ID); // Start command with ID

    char secret_str[33];
    for (int i = 0; i < 16; i++) {
        sprintf(secret_str + (i * 2), "%02x", secret[i]); // Convert secret to hex string
    }
    secret_str[32] = '\0';
    command_len += snprintf(command + command_len, sizeof(command) - command_len, "%s:", secret_str); // Append secret

    command_len += snprintf(command + command_len, sizeof(command) - command_len, "%u:", data_length); // Append data len

    if (data_length > sizeof(command) - command_len - 1) {
        fprintf(stderr, "Error: Data length exceeds command buffer size\n");
        return 1; // Insufficient buffer size
    }

    memcpy(command + command_len, data, data_length); 
    command[command_len + data_length] = '\0'; 

    int fifo_write_fd = open(FIFO_WRITE_PATH, O_WRONLY);
    if (fifo_write_fd == -1) {
        perror("open");
        return 1; 
    }

    if (send_command(command) == -1) { 
        close(fifo_write_fd);
        return 1; 
    }

    close(fifo_write_fd);

    return 0; 
}

/**
 * Prints secret in hex
 * @param secret Pointer to secret array
 * @param size Num. bytes in secret 
 */
void print_secret(uint8_t secret[], size_t size) {
    for (int i = 0; i < size; i++) {
        printf("%02x", secret[i]); // Print w/ hex conversion
    }
    printf("\n"); 
}
