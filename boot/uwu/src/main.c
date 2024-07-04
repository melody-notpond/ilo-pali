#include "syscalls.h"

void _start() {
    while (1)
        uart_puts("from uwu");
}

