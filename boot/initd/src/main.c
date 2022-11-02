#include "fat16.h"
#include "syscalls.h"

/*
bool streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b)
            return false;
        a++;
        b++;
    }

    return *a == *b;
}
*/

void thread(void* data) {
    uart_puts("spawned thread");
    sleep(1, 0);
    *((char*) data) = 1;
    exit(0);
}

void _start() {
    /*
    struct allowed_memory allowed;
    uart_puts("initd started");
    for (size_t i = 0; get_allowed_memory(i, &allowed); i++) {
        if (streq(allowed.name, "initrd")) {
            break;
        }
    }

    void* fat_raw = map_physical_memory(allowed.start, allowed.size, PAGE_PERM_READ | PAGE_PERM_WRITE);
    fat16_fs_t fat = verify_initrd(fat_raw, fat_raw + allowed.size);
    size_t size;
    void* data = read_file_full(&fat, "uwu", &size);
    uart_puts("spawning /uwu");
    spawn(data, size, "uwu", 0, NULL);
    while(1);
    */

    uart_puts("uwu");
    char lock_value = 0;
    spawn_thread(thread, &lock_value);
    if (lock(&lock_value, LOCK_WAIT | LOCK_U8, 0))
        uart_puts("oh no,,,");
    else uart_puts("nya!");

    while(1);
}

