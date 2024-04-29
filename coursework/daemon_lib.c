#include "daemon_lib.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

struct data_block{
    char id[32];
    struct secretRecord *secrets;
    uint32_t data_len;
    void* data;
    time_t last_update;
    struct data_block *next;
};

struct secretRecord {
    uint8_t secret[16];
    uint8_t permissions;
    struct secretRecord *next;
};


uint8_t sendNewBlock(char *ID, uint8_t *secret, uint32_t data_length, void *data) {
    // TODO: send new data block to server
    return 0;
}

uint8_t getBlock(char *ID, uint8_t *secret, uint32_t buffer_size, void *buffer) {
    // TODO: retrieve data block from server
    return 0;
}