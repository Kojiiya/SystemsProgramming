#include "daemon_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int main() {
    //Define the secret key
    uint8_t secret[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

    //Define expected data length 
    uint32_t data_length = 10; //Need to adjust/make dynamic/have daemon send length

    //Allocate buffer to store retrieved data block
    uint32_t buffer_size = 1024; //Adjust based on expected data size from daemon
    char *buffer = malloc(buffer_size);
    if (buffer == NULL || buffer_size == 0) {
        perror("malloc");
        cleanup_socket();
        return 1;
    }

    //Retrieve data block from  daemon
    if (getBlock("TestID123", secret, buffer_size, buffer) == -1) {
        printf("Failed to retrieve data block from daemon\n");
        free(buffer);
        cleanup_socket();
        return 1;
    } else if (buffer_size < data_length) {
        printf("Error: Buffer size is too small to hold the retrieved data block.\n");
        free(buffer);
        cleanup_socket();
        return 1;
    }


    printf("Retrieved data block: %.*s\n", data_length, buffer);

    //free allocation buffer
    free(buffer);

    //socket cleanup
    cleanup_socket();

    return 0;
}
