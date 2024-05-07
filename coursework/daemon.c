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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <time.h>

#define FIFO_PATH "/tmp/daemon_fifo"
#define SHM_KEY 6574
#define MAX_BLOCKS 1024

static pid_t daemon_pid = -1;
static struct data_block *data_blocks = NULL;
static int shm_id = -1;
static int blocks_stored_shmid = -1;
static int *blocks_stored_ptr = NULL;

struct data_block *get_data_blocks() {
    return data_blocks;
}

void cleanup_resources() {
    // Unmap and close the shared memory segment for data_blocks
    munmap(data_blocks, MAX_BLOCKS * sizeof(struct data_block));
    close(shm_id);

    // Unmap and close the shared memory segment for blocks_stored count
    munmap(blocks_stored_ptr, sizeof(int));
    close(blocks_stored_shmid);

    // Iterate over all data blocks to clean up data segments
    for (int i = 0; i < *blocks_stored_ptr; i++) {
        struct data_block *current_block = &data_blocks[i];

        // Unmap the shared memory segment for the DynamicData struct
        munmap(current_block->dynamic_data->data, current_block->dynamic_data->size);

        // Close the file descriptor for the DynamicData struct
        close(current_block->dynamic_data->data_shmid);

        // Remove the shared memory object for the DynamicData struct
        char shm_name[256];
        snprintf(shm_name, sizeof(shm_name), "/data_block_%d", i);
        shm_unlink(shm_name);

        // Free the DynamicData struct
        free(current_block->dynamic_data);
    }

    // Additional cleanup code...
}

void handle_signal(int signum) {
    printf("Received signal %d, cleaning up and exiting...\n", signum);
    cleanup_resources();
    exit(EXIT_SUCCESS);
}

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

void store_data_block(const char *id, const uint8_t *secret, uint8_t permissions, uint32_t data_len, const void *data) {
    if (*blocks_stored_ptr >= MAX_BLOCKS) {
        fprintf(stderr, "Maximum number of blocks reached.\n");
        return;
    }

    struct data_block *new_block = &data_blocks[*blocks_stored_ptr];
    strncpy(new_block->id, id, sizeof(new_block->id) - 1);
    new_block->id[sizeof(new_block->id) - 1] = '\0'; // Ensure null-termination

    DynamicData *dynamicData = malloc(sizeof(DynamicData));
    if (!dynamicData) {
        perror("malloc failed for dynamicData");
        return;
    }

    // Open or create the shared memory object for the data block
    char shm_name[256];
    snprintf(shm_name, sizeof(shm_name), "/data_block_%d", *blocks_stored_ptr);
    int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open failed");
        free(dynamicData);
        return;
    }

    // Set the size of the shared memory object
    if (ftruncate(fd, data_len) == -1) {
        perror("ftruncate failed");
        close(fd);
        free(dynamicData);
        return;
    }

    // Map the shared memory object into this process's address space
    dynamicData->data = mmap(NULL, data_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (dynamicData->data == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        free(dynamicData);
        return;
    }

    dynamicData->size = data_len;
    dynamicData->data_shmid = fd; // Store the file descriptor

    // Copy the data to the shared memory segment
    memcpy(dynamicData->data, data, dynamicData->size);

    // Store the pointer to the DynamicData struct in the data_block
    new_block->dynamic_data = dynamicData;
    new_block->last_update = time(NULL);
    new_block->next = NULL;

    (*blocks_stored_ptr)++;
}

void print_data_blocks() {
    printf("Stored data blocks:\n");
    for (int i = 0; i < *blocks_stored_ptr; i++) {
        struct data_block *current_block = &data_blocks[i];
        printf("Data Block %d:\n", i + 1);
        printf(" ID: %s\n", current_block->id);
        printf(" Data Length: %zu\n", current_block->dynamic_data->size);
        printf(" Data: %.*s\n", (int)current_block->dynamic_data->size, (char *)current_block->dynamic_data->data);
        printf(" Last Update: %s", ctime(&current_block->last_update));
    }

    if (*blocks_stored_ptr == 0) {
        printf("No data blocks stored.\n");
    }
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
    printf("Command: %s\n", token);
    printf("Before If statement\n");
    if (token != NULL && strcmp(token, "SEND_NEW_BLOCK") == 0) {
        printf("In If statement\n");
        char *id = strtok(NULL, ":");
        char *secret_str = strtok(NULL, ":");
        uint32_t data_len = atoi(strtok(NULL, ":"));
        char *data = strtok(NULL, ":");
        printf("Before printing recieved block");
        printf("ID: %s\n", id);
        printf("Secret: %s\n", secret_str);
        printf("Data Length: %u\n", data_len);

        // Null Checks
        if (data == NULL) {
            fprintf(stderr, "Error: Received null data.\n");
            // Handle the error appropriately
            return;
        }
        if (secret_str == NULL || strlen(secret_str) != 32) {
            fprintf(stderr, "Error: Invalid secret string.\n");
            // Handle the error appropriately
            return;
        }

        printf("Data: %s\n", data);
        printf("After printing recieved block\n");
        printf("Secret string: %s\n", secret_str);
        // Convert secret from hex string to uint8_t array
        uint8_t secret[16];
        for (int i = 0; i < 16; i++) {
            printf("%d", i);
            if (sscanf(secret_str + (i * 2), "%2hhx", &secret[i]) != 1) {
                fprintf(stderr, "Error: Failed to parse secret string.\n");
                // Handle the error appropriately
                return;
            }
        }
        printf("\nAfter converting secret to uint8_t\n");
        // Store the new data block
        store_data_block(id, secret, 0, data_len, data);
        print_data_blocks();
        printf("Blocks: %d\n", *blocks_stored_ptr);

        // Send a response back to the client
        char response[] = "DATA_STORED";
        write(client_fd, response, sizeof(response));
    } else if (token != NULL && strcmp(token, "GET_BLOCK") == 0) {
        // Implement GET_BLOCK functionality
        // ...
    } else {
        printf("Unknown command: %s\n", token);
    }

    close(client_fd);
}

int main() {
    initialize_daemon();

    if (create_fifo() == -1) {
        perror("Failed to create FIFO");
        exit(EXIT_FAILURE);
    }

    size_t shm_size = MAX_BLOCKS * sizeof(struct data_block);

    // Open or create the shared memory object for data_blocks
    shm_id = shm_open("/data_blocks", O_CREAT | O_RDWR, 0666);
    if (shm_id == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    // Set the size of the shared memory object
    if (ftruncate(shm_id, shm_size) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    // Create a shared memory segment for blocks_stored count
    blocks_stored_shmid = shm_open("/blocks_stored", O_CREAT | O_RDWR, 0666);
    if (blocks_stored_shmid == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(blocks_stored_shmid, sizeof(int)) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    blocks_stored_ptr = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, blocks_stored_shmid, 0);
    if (blocks_stored_ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    *blocks_stored_ptr = 0;

    // Map the shared memory object into this process's address space
    data_blocks = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_id, 0);
    if (data_blocks == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // Initialize the data_blocks array
    memset(data_blocks, 0, shm_size);

    printf("Attached to shared memory at address: %p\n", (void *)data_blocks);

    daemon_pid = getpid();

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

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
            handle_client(client_fd);
            exit(EXIT_SUCCESS);
        } else {
            close(client_fd);
        }

        while (waitpid(-1, NULL, WNOHANG) > 0);
    }

    cleanup_resources();

    return 0;
}