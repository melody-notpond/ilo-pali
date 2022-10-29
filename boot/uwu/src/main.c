#include "syscalls.h"

void fault_handler(int cause, uint64_t pc, uint64_t sp, uint64_t fp) {
    (void) cause;
    (void) pc;
    (void) sp;
    (void) fp;
    uart_puts("oh no i faulted");
}

int _start() {
    uart_puts("uwu");
    set_fault_handler(fault_handler);
    *((volatile char*) 0) = 0;
    while(1);
}

