#ifndef DAEMON_LIB_H
#define DAEMON_LIB_H

#include <stdint.h>
#include <time.h>

extern int fifo_fd;
struct data_block *get_data_blocks();
void print_secret(uint8_t *secret, size_t size);
struct secretRecord {
    uint8_t secret[16];
    uint8_t permissions;
    struct secretRecord *next;
};

struct data_block {
    char id[32];
    struct secretRecord *secrets;
    uint32_t data_len;
    void *data;
    time_t last_update;
    struct data_block *next;
};

uint8_t sendNewBlock(char *ID, uint8_t *secret, uint32_t data_length, void *data);
uint8_t getBlock(char *ID, uint8_t *secret, uint32_t buffer_size, void *buffer);
int send_command(const char *command, int *fifo_fd_ptr);
void initialize_daemon();

#endif