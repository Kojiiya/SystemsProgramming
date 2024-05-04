// test_receive.c
#include "daemon_lib.h"
#include <stdio.h>
#include <string.h>

int main() {
    char *ID = "testblock";
    uint8_t secret[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    char buffer[1024];
    uint32_t buffer_size = sizeof(buffer);

    uint32_t result = getBlock(ID, secret, buffer_size, buffer);

    if (result >= 0) {
        printf("Data received from daemon: %.*s\n", result, buffer);
    } else {
        switch (result) {
            case -1:
                printf("Error: Failed to send command to daemon.\n");
                break;
            case -2:
                printf("Error: Failed to read response from daemon.\n");
                break;
            case -3:
                printf("Error: No response received from daemon.\n");
                break;
            case -4:
                printf("Error: Buffer too small\n");
                break;
            case -5:
                printf("Error: Failed to read data from daemon.\n");
                break;
            case -6:
                printf("Error: No data received from daemon.\n");
                break;
            case -7:
                printf("Error: Unexpected response received from daemon.\n");
                break;
            default:
                printf("Unknown error occurred: %d\n", result);
                break;
        }
    }

    return 0;
}
