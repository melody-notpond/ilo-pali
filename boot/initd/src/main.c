#include "syscalls.h"

int _start() {
    uart_puts("hewo!");

    int* x = page_alloc(1, PAGE_PERM_READ | PAGE_PERM_WRITE);
    *x = 2;
    if (page_dealloc(x, 1))
        uart_puts("uwu!");
    if (page_dealloc((void*) 0, 1))
        uart_puts("owo!");
    x = page_alloc(0, PAGE_PERM_READ | PAGE_PERM_WRITE);
    *x = 2;
    while(1);
}

