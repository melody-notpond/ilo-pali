#include <stddef.h>

#include "syscalls.h"

void _start(char* msg, size_t size) {
    uart_write(msg, size);
    uint64_t data;
    size_t meta;
    recv(true, NULL, NULL, &data, &meta);
    while (1) {
        uart_write(msg, size);
        sleep(1, 0);
        *(uint8_t*) data = *(uint8_t*) data + 1;
    }
}
