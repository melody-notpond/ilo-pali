#include "syscalls.h"

void _start() {
    uart_write("spawned virtd!\n", 15);
    while(1);
}
