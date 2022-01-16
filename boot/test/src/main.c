#include <stddef.h>

#include "syscall.h"

void _start(char* msg, size_t size) {
    uart_write(msg, size);
    uint64_t data;
    size_t meta;
    recv(true, NULL, NULL, &data, &meta);
    while (1) {
        uart_write((void*) data, meta);
        sleep(2, 0);
    }
}
