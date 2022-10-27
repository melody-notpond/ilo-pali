//#include "fat16.h"
#include "syscalls.h"

int _start() {
    struct allowed_memory allowed;
    size_t i = 0;
    uart_puts("uwu!");
    while (get_allowed_memory(i, &allowed)) {
        uart_puts(allowed.name);
        i++;
    };
    uart_puts("owo!");
    while(1);
}

