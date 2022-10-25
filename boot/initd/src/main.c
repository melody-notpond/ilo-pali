#include "syscalls.h"

int _start() {
    uart_puts("hewo!");
    while(1) {
        uart_puts("a");
        sleep(1, 0);
    }
}

