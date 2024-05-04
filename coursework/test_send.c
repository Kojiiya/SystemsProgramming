//test_send.c
#include "daemon_lib.h"
#include <stdio.h>
#include <string.h>

int main() {
    char *ID = "testblock";
    uint8_t secret[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    char *data = "This is a test data block.";
    uint32_t data_length = strlen(data);

    printf("Testing sendNewBlock function...\n");
    printf("ID: %s\n", ID);
    printf("Secret: ");
    print_secret(secret, sizeof(secret));
    printf("Data: %s\n", data);
    printf("Data Length: %u\n", data_length);

    uint8_t result = sendNewBlock(ID, secret, data_length, data);

    switch (result) {
        case 0:
            printf("Block sent and stored successfully.\n");
            break;
        case 1:
            printf("Failed to send command to daemon.\n");
            break;
        case 3:
            printf("Failed to read response from daemon.\n");
            break;
        case 4:
            printf("No response received from daemon.\n");
            break;
        case 5:
            printf("Error response received from daemon.\n");
            break;
        default:
            printf("Unknown error occurred.\n");
    }
    return 0;
}