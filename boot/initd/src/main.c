//#include "fat16.h"
#include "syscalls.h"

int _start() {
    struct allowed_memory allowed;
    uart_puts("initd started");
    for (size_t i = 0; get_allowed_memory(i, &allowed); i++) {
        if (allowed.name[0] == 'f' && allowed.name[1] == 'd' && allowed.name[2] == 't' && allowed.name[3] == '\0') {
            break;
        }
    }

    void* fdt = map_physical_memory(allowed.start, allowed.size, PAGE_PERM_READ | PAGE_PERM_WRITE);
    *(int*) fdt = 0;
    uart_puts("nya");
    while(1);
}

