#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>

#define SOCKET_PATH "/tmp/daemon_socket"

int main() {
    int sockfd;
    struct sockaddr_un addr;

    //socket creation
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    //connect to daemon
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
        perror("connect");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    //send tst msg to daemon
    const char *test_message = "This is a test message from the client.";
    if (write(sockfd, test_message, strlen(test_message)) == -1) {
        perror("write");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Test message sent to daemon.\n");


    close(sockfd);

    return 0;
}
