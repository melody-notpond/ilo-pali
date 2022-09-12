#include <stdint.h>

#include "syscalls.h"

uint64_t syscall(uint64_t call, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

// uart_puts(char*) -> void
// Prints out a message onto the UART.
void uart_puts(char* msg) {
    syscall(0, (uint64_t) msg, 0, 0, 0, 0, 0);
}
