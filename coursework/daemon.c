//daemon.c
#include "daemon_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#define FIFO_PATH "/tmp/daemon_fifo"

static pid_t daemon_pid = -1;
static struct data_block *data_blocks = NULL;

struct data_block *get_data_blocks() {
    return data_blocks;
}
void handle_client(int client_fd);
void cleanup_fifo_and_exit(int signum);
void store_data_block(const char *id, const uint8_t *secret, uint8_t permissions, uint32_t data_len, const void *data);

int create_fifo() {
    // Remove any existing FIFO
    unlink(FIFO_PATH);

    // Create a new FIFO
    if (mkfifo(FIFO_PATH, 0666) == -1) {
        perror("mkfifo");
        return -1;
    }

    return 0;
}

void cleanup_fifo() {
    unlink(FIFO_PATH);
}

void cleanup_fifo_and_exit(int signum) {
    printf("Received signal %d, cleaning up and exiting...\n", signum);
    cleanup_fifo();

    // Free the allocated memory
    struct data_block *current_block = data_blocks;
    while (current_block != NULL) {
        struct data_block *temp_block = current_block;
        current_block = current_block->next;

        struct secretRecord *current_secret = temp_block->secrets;
        while (current_secret != NULL) {
            struct secretRecord *temp_secret = current_secret;
            current_secret = current_secret->next;
            free(temp_secret);
        }

        free(temp_block->data);
        free(temp_block);
    }

    exit(EXIT_SUCCESS);
}

void handle_client(int client_fd) {
    char buffer[1024];
    ssize_t num_bytes = read(client_fd, buffer, sizeof(buffer) - 1);

    if (num_bytes == -1) {
        perror("read");
        close(client_fd);
        exit(EXIT_FAILURE);
    } else if (num_bytes == 0) {
        close(client_fd);
        exit(EXIT_SUCCESS);
    }

    buffer[num_bytes] = '\0';
    printf("Received command from client: %s\n", buffer);

    // Parse the command
    char *token = strtok(buffer, ":");
    printf("Command: %s\n", token); // Print the command

    if (strcmp(token, "SEND_NEW_BLOCK") == 0) {
        char *id = strtok(NULL, ":");
        char *secret_str = strtok(NULL, ":");
        uint32_t data_len = atoi(strtok(NULL, ":"));
        char *data = strtok(NULL, ":");

        printf("ID: %s\n", id);
        printf("Secret: %s\n", secret_str);
        printf("Data Length: %u\n", data_len);
        printf("Data: %s\n", data);

        // Convert secret from hex string to uint8_t array
        uint8_t secret[16];
        for (int i = 0; i < 16; i++) {
            sscanf(secret_str + (i * 2), "%2hhx", &secret[i]);
        }

        // Store the new data block
        store_data_block(id, secret, 0, data_len, data);

        // Send a response back to the client
        char response[] = "DATA_STORED";
        write(client_fd, response, sizeof(response));
    } else if (strcmp(token, "GET_BLOCK") == 0) {
        char *id = strtok(NULL, ":");
        char *secret_str = strtok(NULL, ":");

        printf("ID: %s\n", id);
        printf("Secret: %s\n", secret_str);

        uint8_t secret[16];
        for (int i = 0; i < 16; i++) {
            sscanf(secret_str + (i * 2), "%2hhx", &secret[i]);
        }

        struct data_block *current_block = data_blocks;
        while (current_block != NULL) {
            if (strcmp(current_block->id, id) == 0 && memcmp(current_block->secrets->secret, secret, 16) == 0) {
                // Open the FIFO for writing
                int client_fd_write = open(FIFO_PATH, O_WRONLY);
                if (client_fd_write == -1) {
                    perror("Failed to open FIFO for writing");
                    close(client_fd);
                    exit(EXIT_FAILURE);
                }

                // Construct the response header with the data length
                char response_header[64];
                sprintf(response_header, "DATA:%u:", current_block->data_len);
                if (write(client_fd_write, response_header, strlen(response_header)) == -1) {
                    perror("write");
                    close(client_fd);
                    close(client_fd_write);
                    exit(EXIT_FAILURE);
                }

                // Write the data block to the FIFO
                if (write(client_fd_write, current_block->data, current_block->data_len) == -1) {
                    perror("write");
                    close(client_fd);
                    close(client_fd_write);
                    exit(EXIT_FAILURE);
                }

                close(client_fd);
                close(client_fd_write);
                break;
            }
            current_block = current_block->next;
        }

        if (current_block == NULL) {
            printf("Data block not found\n");
            close(client_fd);
        }
    } else {
        close(client_fd);
    }

    exit(EXIT_SUCCESS);
}



void store_data_block(const char *id, const uint8_t *secret, uint8_t permissions, uint32_t data_len, const void *data) {
    struct data_block *new_block = malloc(sizeof(struct data_block));
    strncpy(new_block->id, id, sizeof(new_block->id));

    struct secretRecord *new_secret = malloc(sizeof(struct secretRecord));
    memcpy(new_secret->secret, secret, sizeof(new_secret->secret));
    new_secret->permissions = permissions;
    new_secret->next = NULL;

    new_block->secrets = new_secret;
    new_block->data_len = data_len;
    new_block->data = malloc(data_len);
    memcpy(new_block->data, data, data_len);
    new_block->last_update = time(NULL);
    new_block->next = NULL;

    // Add the new block to the linked list
    if (data_blocks == NULL) {
        data_blocks = new_block;
    } else {
        struct data_block *current = data_blocks;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_block;
    }
}

int main() {
    initialize_daemon();

    // Create the FIFO
    if (create_fifo() == -1) {
        perror("Failed to create FIFO");
        exit(EXIT_FAILURE);
    }

    daemon_pid = getpid();

    signal(SIGINT, cleanup_fifo_and_exit);
    signal(SIGTERM, cleanup_fifo_and_exit);

    // Main loop to accept and handle connections
    while (1) {
        printf("Daemon is listening for incoming connections...\n");
        int client_fd = open(FIFO_PATH, O_RDONLY);
        if (client_fd == -1) {
            perror("Failed to open FIFO");
            continue;
        }

        pid_t child_pid = fork();
        if (child_pid == -1) {
            perror("fork");
            close(client_fd);
            continue;
        } else if (child_pid == 0) {
            // Child process
            handle_client(client_fd);
            // Child process exits after handling the client
        } else {
            // Parent process
            close(client_fd);
            // Parent process continues to accept new connections
        }

        // Reap zombie child processes
        while (waitpid(-1, NULL, WNOHANG) > 0);
    }

    // Cleanup the FIFO when the program exits
    cleanup_fifo();

    // Print "Daemon waiting for incoming connections..." once at the end
    printf("Daemon waiting for incoming connections...\n");

    return 0;
}