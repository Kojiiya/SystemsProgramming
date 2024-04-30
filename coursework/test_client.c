//test_client.c
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
#define MAX_RETRIES 10
#define RETRY_INTERVAL 1 // in seconds

int connect_to_daemon() {
    int sockfd;
    struct sockaddr_un addr;

    for (int i = 0; i < MAX_RETRIES; i++) {
        // Attempt to create a socket
        if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            return -1;
        }

        // Initialize address structure
        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

        // Attempt to connect to the daemon
        if (connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == 0) {
            // Connection successful
            return sockfd;
        } else {
            // Connection failed
            if (errno == ENOENT) {
                // Socket file doesn't exist yet, retry after a short interval
                close(sockfd);
                sleep(RETRY_INTERVAL);
                continue;
            } else {
                // Other error occurred
                perror("connect");
                close(sockfd);
                return -1;
            }
        }
    }

    // Max retries reached, unable to connect
    fprintf(stderr, "Unable to connect to the daemon after %d retries.\n", MAX_RETRIES);
    return -1;
}
int main() {
    // Wait for the daemon to start
    printf("Waiting for the daemon to start...\n");

    int sockfd = connect_to_daemon();
    if (sockfd == -1) {
        fprintf(stderr, "Failed to connect to the daemon.\n");
        exit(EXIT_FAILURE);
    }

    printf("Daemon started. Connected.\n");

    // Send a test command to the daemon
    printf("Sending test command to daemon...\n");
    if (send_command("COMMAND:SEND") == -1) {
        fprintf(stderr, "Failed to send test command to daemon.\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Test command sent.\n");

    // Close the connection to the daemon
    close(sockfd);

    return 0;
}