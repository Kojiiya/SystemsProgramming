//daemon_lib.c
#include "daemon_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h> 

#define SOCKET_PATH "/tmp/daemon_socket"
#define STORAGE_FILE "/tmp/daemon_storage"

int sockfd = -1;

void initialize_daemon() {
    printf("Daemon starting\n");
    sockfd = -1;
    //can open socket here
}

//socket cleanup
void cleanup_socket() {
    if (sockfd!= -1) {
        close(sockfd);
        sockfd = -1; //-1 indicates that the socket is closed
    }
    unlink(SOCKET_PATH);
}

int send_command(const char *command) {
    struct sockaddr_un addr;

    //Check if socket is already open
    if (sockfd == -1) {
        //Create socket if not
        if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            return -1;
        }

        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

        //Connect to daemon
        if (connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
            perror("connect");
            close(sockfd);
            sockfd = -1;
            return -1;
        }
    }

    //Send command to daemon
    if (write(sockfd, command, strlen(command)) == -1) {
        perror("write");
        close(sockfd);
        sockfd = -1; 
        return -1;
    }

    return 0;
}


uint8_t sendNewBlock(char *ID, uint8_t *secret, uint32_t data_length, void *data) {
    //Construct command
    char command[1024];
    sprintf(command, "SEND_NEW_BLOCK:%s:%016lx:", ID, *(uint64_t *)secret);

    //Send command to daemon
    if (send_command(command) == -1) {
        return -1; //Command send failure
    }

    //Send data_length bytes of data
    if (write(sockfd, data, data_length) == -1) {
        perror("write");
        close(sockfd);
        return -2; //Data send failure
    }

    //Assuming the daemon responds with a success message or an error message
    char response[1024];
    ssize_t num_bytes = read(sockfd, response, sizeof(response) - 1);
    if (num_bytes == -1) {
        perror("read");
        close(sockfd);
        return -3; // Read failure
    } else if (num_bytes == 0) {
        printf("No response received from daemon\n");
        close(sockfd);
        return -4; // No response received
    } else {
        response[num_bytes] = '\0'; //Null-terminate the received message
        printf("Response from daemon: %s\n", response);
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
        printf("Data received from daemon: %.*s\n", (int)num_bytes, (char *)buffer);
        return 1; // Success
    }
}