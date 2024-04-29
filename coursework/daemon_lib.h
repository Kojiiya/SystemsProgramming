#ifndef DAEMON_LIB_H
#define DAEMON_LIB_H

#include <stdint.h>

uint8_t sendNewBlock(char *ID, uint8_t *secret, uint32_t data_length, void *data);
uint8_t getBlock(char *ID, uint8_t *secret, uint32_t buffer_size, void *buffer);

#endif 
