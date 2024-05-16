#include "daemon_lib.h" 
#include <stdio.h>   
#include <stdlib.h>    
#include <unistd.h>  
#include <sys/types.h> 
#include <sys/stat.h>  // File creation mask
#include <fcntl.h>     // File control 
#include <string.h>    
#include <signal.h>    // Sig mgmnt
#include <sys/wait.h>   // Process control
#include <sys/ipc.h>   
#include <sys/shm.h>    // Shared memory
#include <sys/mman.h>   // Memory mapping
#include <time.h>       // Time/date 
#include <pthread.h>    // Thread creation/synchronization
#include <errno.h>      // Errors

#define FIFO_READ_PATH "/tmp/daemon_fifo_read" // Path for reading FIFO
#define FIFO_WRITE_PATH "/tmp/daemon_fifo_write" // Path for writing FIFO
#define SHM_KEY 6574 
#define MAX_BLOCKS 1024 

static pid_t daemon_pid = -1;
static struct data_block *data_blocks = NULL; 
static int blocks_stored_shmid = -1; 
static int *blocks_stored_ptr = NULL; 
pthread_mutex_t mutex; 


struct data_block *get_data_blocks() {
    return data_blocks;
}


void free_secret_records(struct secretRecord *head) {
    struct secretRecord *current = head;
    struct secretRecord *next;

    while (current!= NULL) {
        next = current->next;
        free(current);
        current = next;
    }
}

// Function to initialize the daemon, including creating FIFOs and initializing mutex
void initialize_daemon() {
    printf("Daemon starting\n");
    // Create both FIFOs
    if (mkfifo(FIFO_WRITE_PATH, 0666) == -1 && errno!= EEXIST) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
    if (mkfifo(FIFO_READ_PATH, 0666) == -1 && errno!= EEXIST) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_init(&mutex, NULL);

    return;
}

// Function to clean up resources like shared memory segments and secret records
void cleanup_resources() {
    // Iterate over all data blocks to clean up data segments and secret records
    for (int i = 0; i < *blocks_stored_ptr; i++) {
        struct data_block *current_block = &data_blocks[i];

        // Unmap the shared memory segment for the DynamicData struct
        munmap(current_block->dynamic_data, sizeof(DynamicData));

        // Close and unlink the shared memory segment for the DynamicData struct
        char shm_name[256];
        snprintf(shm_name, sizeof(shm_name), "/dynamic_data_%d", i);
        shm_unlink(shm_name);

        // Unmap the shared memory segment for the DynamicData struct's data
        munmap(current_block->dynamic_data->data, current_block->dynamic_data->size);

        // Close and unlink the shared memory segment for the data
        char data_shm_name[256];
        snprintf(data_shm_name, sizeof(data_shm_name), "/data_block_%d", i);
        shm_unlink(data_shm_name);

        // Free the secretRecord linked list
        free_secret_records(current_block->secrets);
    }

    // After cleaning up individual data blocks, unmap and close the shared memory segments
    if (data_blocks!= NULL) {
        munmap(data_blocks, MAX_BLOCKS * sizeof(struct data_block));
        shm_unlink("/data_blocks");
    }

    if (blocks_stored_ptr!= NULL) {
        munmap(blocks_stored_ptr, sizeof(int));
        close(blocks_stored_shmid);
        shm_unlink("/blocks_stored");
    }

    pthread_mutex_destroy(&mutex);
    return;
}

// Function to handle signals for graceful shutdown
void handle_signal(int signum) {
    printf("Received signal %d, cleaning up and exiting...\n", signum);
    cleanup_resources();
    exit(EXIT_SUCCESS);
}

// Function to create FIFOs for inter-process communication
int create_fifo() {
    unlink(FIFO_READ_PATH);
    unlink(FIFO_WRITE_PATH);

    if (mkfifo(FIFO_READ_PATH, 0666) == -1 || mkfifo(FIFO_WRITE_PATH, 0666) == -1) {
        perror("mkfifo");
        return -1;
    }

    return 0;
}

// Function to store a new data block with its associated metadata
void store_data_block(const char *id, const uint8_t *secret, uint8_t permissions, uint32_t data_len, const void *data) {
    if (*blocks_stored_ptr >= MAX_BLOCKS) {
        fprintf(stderr, "Maximum number of blocks reached.\n");
        return;
    }

    struct data_block *new_block = &data_blocks[*blocks_stored_ptr];
    strncpy(new_block->id, id, sizeof(new_block->id) - 1);
    new_block->id[sizeof(new_block->id) - 1] = '\0';

    char shm_name[256];
    snprintf(shm_name, sizeof(shm_name), "/dynamic_data_%d", *blocks_stored_ptr);
    int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open failed");
        return;
    }

    if (ftruncate(fd, sizeof(DynamicData)) == -1) {
        perror("ftruncate failed");
        close(fd);
        return;
    }

    DynamicData *dynamicData = mmap(NULL, sizeof(DynamicData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (dynamicData == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return;
    }

    // Verify that dynamicData pointer is not NULL
    if (dynamicData == NULL) {
        fprintf(stderr, "Failed to allocate memory for dynamicData.\n");
        close(fd);
        return;
    }

    char data_shm_name[256];
    snprintf(data_shm_name, sizeof(data_shm_name), "/data_block_%d", *blocks_stored_ptr);
    int data_fd = shm_open(data_shm_name, O_CREAT | O_RDWR, 0666);
    if (data_fd == -1) {
        perror("shm_open failed");
        munmap(dynamicData, sizeof(DynamicData));
        close(fd);
        return;
    }

    if (ftruncate(data_fd, data_len) == -1) {
        perror("ftruncate failed");
        close(data_fd);
        munmap(dynamicData, sizeof(DynamicData));
        close(fd);
        return;
    }

    dynamicData->data = mmap(NULL, data_len, PROT_READ | PROT_WRITE, MAP_SHARED, data_fd, 0);
    if (dynamicData->data == MAP_FAILED) {
        perror("mmap failed");
        close(data_fd);
        munmap(dynamicData, sizeof(DynamicData));
        close(fd);
        return;
    }

    // Verify that the data field of dynamicData is properly initialized and accessible
    if (dynamicData->data == MAP_FAILED) {
        fprintf(stderr, "Failed to initialize data field of dynamicData.\n");
        close(data_fd);
        munmap(dynamicData, sizeof(DynamicData));
        close(fd);
        return;
    }

    dynamicData->size = data_len;
    memcpy(dynamicData->secret, secret, 16);
    printf("Stored Secret Address %p\n", (void *) dynamicData->secret);
    dynamicData->data_shmid = data_fd;

    memcpy(dynamicData->data, data, dynamicData->size);

    new_block->dynamic_data = dynamicData;
    printf("Stored dynamicData Address: %p\n", (void *) dynamicData);
    new_block->last_update = time(NULL);
    new_block->next = NULL;

    // Initialize the secretRecord linked list
    struct secretRecord *new_secret_record = malloc(sizeof(struct secretRecord));
    if (new_secret_record == NULL) {
        perror("malloc failed for secretRecord");
        // Handle error appropriately
        return;
    }
    memcpy(new_secret_record->secret, secret, 16);
    new_secret_record->permissions = permissions;
    new_secret_record->next = NULL;
    new_block->secrets = new_secret_record;
    
    (*blocks_stored_ptr)++;

    pthread_mutex_unlock(&mutex);
}

// Function to print details of all stored data blocks
void print_data_blocks() {
    printf("Stored data blocks:\n");
    for (int i = 0; i < *blocks_stored_ptr; i++) {
        struct data_block *current_block = &data_blocks[i];
        printf("Data Block %d:\n", i + 1);
        printf(" ID: %s\n", current_block->id);
        printf(" Data Length: %zu\n", current_block->dynamic_data->size);
        printf(" Data: %.*s\n", (int)current_block->dynamic_data->size, (char *)current_block->dynamic_data->data);
        printf(" Secret: ");
        print_secret(current_block->dynamic_data->secret, sizeof(current_block->dynamic_data->secret));
        printf(" Last Update: %s", ctime(&current_block->last_update));
    }

    if (*blocks_stored_ptr == 0) {
        printf("No data blocks stored.\n");
    }
}

// Function to handle incoming commands to add a new data block
void handle_send_new_block(char *command, int client_fd) {
    char *id = strtok(NULL, ":");
    char *secret_str = strtok(NULL, ":");
    uint32_t data_len = atoi(strtok(NULL, ":"));
    char *data = strtok(NULL, ":");

    if (data == NULL) {
        fprintf(stderr, "Error: Received null data.\n");
        return;
    }
    if (secret_str == NULL || strlen(secret_str)!= 32) {
        fprintf(stderr, "Error: Invalid secret string.\n");
        return;
    }

    printf("ID: %s\n", id);
    printf("Secret: %s\n", secret_str);
    printf("Data Length: %u\n", data_len);
    printf("Data: %s\n", data);

    uint8_t secret[16];
    for (int i = 0; i < 16; i++) {
        if (sscanf(secret_str + (i * 2), "%2hhx", &secret[i])!= 1) {
            fprintf(stderr, "Error: Failed to parse secret string.\n");
            return;
        }
    }

    store_data_block(id, secret, 0, data_len, data);
    print_data_blocks();
    printf("Blocks: %d\n", *blocks_stored_ptr);

    char response[] = "DATA_STORED";
    write(client_fd, response, sizeof(response));
}


void handle_get_block(char *command, int client_fd) {
    printf("Handling GET_BLOCK command...\n"); 

    // Extract ID/secret from  command
    char *id = strtok(NULL, ":");
    char *secret_str = strtok(NULL, ":");

    // Validate secret format
    if (secret_str == NULL || strlen(secret_str)!= 32) {
        fprintf(stderr, "Error: Invalid secret string.\n");
        write(client_fd, "INVALID_SECRET", 14); 
        return;
    }

    // Convert hex secret string into byte array
    uint8_t secret[16];
    for (int i = 0; i < 16; i++) {
        if (sscanf(secret_str + (i * 2), "%2hhx", &secret[i])!= 1) {
            fprintf(stderr, "Error: Failed to parse secret string.\n");
            write(client_fd, "INVALID_SECRET", 14); 
            return;
        }
    }

    printf("We there yet?\n"); 
  
    int result = -1; 
    void *buffer = NULL;
    
    pthread_mutex_lock(&mutex);

    // Search through stored blocks
    for (int i = 0; i < *blocks_stored_ptr; i++) {
        struct data_block *current_block = &data_blocks[i];

        if (current_block->dynamic_data == NULL) {
            printf("Dynamic data pointer is NULL.\n");
            continue;
        }

        // Debugging info about current block
        printf("id: %s\n", current_block->id);
        printf("id address: %p\n", (void *) current_block->id);
        printf("dynamicData Address: %p\n", (void *) current_block->dynamic_data);

        // Check if pointer to dynamic_data is not NULL
        if (current_block->dynamic_data!= NULL) {
            printf("Secret: ");
            print_secret(current_block->dynamic_data->secret, sizeof(current_block->dynamic_data->secret));
        } else {
            printf("Dynamic data pointer is NULL.\n");
        }

        // Compare block's ID/secret
        if (strcmp(current_block->id, id) == 0 && current_block->dynamic_data!= NULL) {
            buffer = current_block->dynamic_data->data;
            result = current_block->dynamic_data->size; 
            break; 
        }
    }

    pthread_mutex_unlock(&mutex);

    if (result >= 0) {
        printf("Data block retrieved successfully.\n");
        char response[1024]; 
        char length_str[16]; 
        sprintf(length_str, "%u", result);
        snprintf(response, sizeof(response), "DATA:%s:%.*s", length_str, result, (char *)buffer);
        write(client_fd, response, strlen(response));
    } else {
        printf("Access denied for the requested data block.\n");
        char response[] = "ACCESS_DENIED"; 
        write(client_fd, response, sizeof(response)); 
    }
}



// Function to handle a single client connection
void handle_client(int client_fd) {
    printf("Handling New Client...\n");

    char buffer[1024]; 
    ssize_t num_bytes = read(client_fd, buffer, sizeof(buffer) - 1);
    if (num_bytes == -1) { 
        perror("read");
        close(client_fd); 
        exit(EXIT_FAILURE); 
    }

    printf("Buffer: %s\n", buffer); 
    buffer[num_bytes] = '\0';

    printf("Received command from client: %s\n", buffer); 

    char *token = strtok(buffer, ":"); 
    printf("Command: %s\n", token); 

    if (token!= NULL && strcmp(token, "SEND_NEW_BLOCK") == 0) {
        handle_send_new_block(buffer, client_fd); 
    } else if (token!= NULL && strcmp(token, "GET_BLOCK") == 0) {
        handle_get_block(buffer, client_fd); 
    } else {
        printf("Unknown command: %s\n", token); 
    }

    close(client_fd);
}



int main() {
    initialize_daemon();

    // Attempt to create FIFO for client communication
    if (create_fifo() == -1) { 
        perror("Failed to create FIFO");
        exit(EXIT_FAILURE); 
    }

    size_t shm_size = MAX_BLOCKS * sizeof(struct data_block); // Calculate required shared memory size

    int shm_fd = shm_open("/data_blocks", O_CREAT | O_RDWR, 0666); // Open/create shared memory object
    if (shm_fd == -1) { 
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shm_fd, shm_size) == -1) { // Resize shared memory object
        perror("ftruncate");
        close(shm_fd); 
        exit(EXIT_FAILURE); 
    }

    data_blocks = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0); // Map shared memory
    if (data_blocks == MAP_FAILED) {
        perror("mmap");
        close(shm_fd); 
        exit(EXIT_FAILURE); 
    }

    close(shm_fd); 

    // Open/create another shared memory object for storing block count
    blocks_stored_shmid = shm_open("/blocks_stored", O_CREAT | O_RDWR, 0666); 
    if (blocks_stored_shmid == -1) { 
        perror("shm_open");
        exit(EXIT_FAILURE); 
    }
    // Resize shared memory object for block count
    if (ftruncate(blocks_stored_shmid, sizeof(int)) == -1) { 
        perror("ftruncate");
        exit(EXIT_FAILURE); 
    }

    // Map shared memory for block count
    blocks_stored_ptr = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, blocks_stored_shmid, 0); 
    if (blocks_stored_ptr == MAP_FAILED) { 
        perror("mmap");
        exit(EXIT_FAILURE); 
    }

    *blocks_stored_ptr = 0; // Initialize block count to zero

    memset(data_blocks, 0, shm_size); 

    daemon_pid = getpid(); 

    signal(SIGINT, handle_signal); 
    signal(SIGTERM, handle_signal); 

    while (1) { 
        printf("Daemon is listening for incoming connections...\n"); 
        int client_fd = open(FIFO_WRITE_PATH, O_RDONLY);
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
            printf("Child has forked\n"); 
            handle_client(client_fd); 
            exit(EXIT_SUCCESS); 
        } else {
            close(client_fd); 
        }

        while (waitpid(-1, NULL, WNOHANG) > 0); // Reap zombies
    }

    cleanup_resources(); 

    return 0; 
}
