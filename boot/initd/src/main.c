#include "syscalls.h"

int _start(int argc, char** argv) {
    uart_puts("hewo!");
    if (argc && argv)
        uart_puts("sjknbjdsf");
    for (int i = 0; i < argc; i++) {
        uart_puts(argv[i]);
    }
    uart_puts("uwu!");
    while(1);
}

