#include "syscalls.h"

int _start() {
    uart_puts("hewo!");
    while(1);
}
