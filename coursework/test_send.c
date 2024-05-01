#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/daemon_socket"

int main() {
    //socket creation
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    //address structure setup
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    //connect to daemon
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("Connected to daemon.\n");

    //send msg
    const char *message = "Hello, World!";
    if (write(sockfd, message, strlen(message)) == -1) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    printf("Message sent to daemon.\n");

    //close socket
    close(sockfd);

    return 0;
}
