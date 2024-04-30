#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <signal.h>

#define SOCKET_PATH "/tmp/daemon_socket"

// Global variable to indicate whether the program should exit
volatile sig_atomic_t should_exit = 0;

// Signal handler function
void sigint_handler(int signum) {
    should_exit = 1;
}

int main(void) {
    int sockfd;
    struct sockaddr_un addr;
    char buf[1024];

    // Register the signal handler for SIGINT
    signal(SIGINT, sigint_handler);

    // Create a Unix domain socket
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set SO_REUSEADDR option
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    // Bind the socket to a local address
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(sockfd, 5) == -1) {
        perror("listen");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Accept incoming connections and handle them
    while (!should_exit) {
        int clientfd;
        struct sockaddr_un client_addr;
        socklen_t client_addr_len = sizeof(struct sockaddr_un);

        if ((clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len)) == -1) {
            if (should_exit) {
                // Exit gracefully if the program should exit
                break;
            }
            perror("accept");
            continue;
        }

        // Receive message from the client
        ssize_t num_bytes = recv(clientfd, buf, sizeof(buf), 0);
        if (num_bytes == -1) {
            perror("recv");
        } else if (num_bytes == 0) {
            printf("Client disconnected\n");
        } else {
            buf[num_bytes] = '\0';  // Null-terminate the received message
            printf("Received message from client: %s\n", buf);
            close(clientfd);
        }

        // Close the client socket
        close(clientfd);
    }

    // Close the server socket before exiting
    close(sockfd);
    printf("daemon has finished");
    return 0;
}
