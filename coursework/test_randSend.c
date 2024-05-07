#include "daemon_lib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Function to generate a random ID of given length
char *generateRandomID(int length) {
    char *id = malloc(length + 1); // Allocate memory for the ID (+1 for null terminator)
    if (id == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Define the character set from which to choose random characters
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    // Seed the random number generator with the current time
    srand(time(NULL));

    // Generate random characters and append them to the ID
    for (int i = 0; i < length; i++) {
        id[i] = charset[rand() % (sizeof(charset) - 1)]; // Select a random character from the charset
    }
    id[length] = '\0'; // Null terminate the string

    return id;
}

int main() {
    char *ID = generateRandomID(10); // Generate a random ID of 10 characters
    uint8_t secret[16]; // Assuming a 16-byte secret
    char *data = "This is a test data block.";
    uint32_t data_length = strlen(data);

    // Seed the random number generator with the current time
    srand(time(NULL));

    // Generate a random secret
    for (int i = 0; i < sizeof(secret); i++) {
        secret[i] = rand() % 256; // Generate a random byte
    }

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
        case 100:
            printf("Error: Daemon is not running.\n");
            break;
        default:
            printf("Unknown error occurred.\n");
    }

    free(ID); // Free the memory allocated for the random ID
    return 0;
}
