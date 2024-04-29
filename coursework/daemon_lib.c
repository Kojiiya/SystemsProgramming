#include "daemon_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>

#define SOCKET_PATH "/tmp/daemon_socket"

//func to send cmd to daemon
int send_command(const char *command) {
    int sockfd;
    struct sockaddr_un addr;

    //create socket
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    //connect to daemon
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    //send cmd to daemon
    if (write(sockfd, command, strlen(command)) == -1) {
        perror("write");
        close(sockfd);
        return -1;
    }

    close(sockfd);
    return 0;
}

//func to send data blk to daemon
uint8_t sendNewBlock(char *ID, uint8_t *secret, uint32_t data_length, void *data) {
    //construct cmd
    char command[1024];
    sprintf(command, "SEND_NEW_BLOCK:%s:%d:", ID, data_length);

    // Send the command to the daemon
    if (send_command(command) == -1) {
        return 0; // Failed to send command
    }

    //TODO
    //send data to daemon

    return 1; //yay it worked
}

//func get data blk from daemon
uint8_t getBlock(char *ID, uint8_t *secret, uint32_t buffer_size, void *buffer) {
    //construct cmd
    char command[1024];
    sprintf(command, "GET_BLOCK:%s:%d:", ID, buffer_size);

    //send cmd to daemon
    if (send_command(command) == -1) {
        return 0; //failure
    }

    //TODO
    //receive the data from the daemon 

    return 1; //yay it worked
}
