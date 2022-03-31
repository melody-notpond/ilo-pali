#include <stdatomic.h>
#include <stddef.h>

#include "fdt.h"
#include "iter/string.h"
#include "iter/vec.h"
#include "sync.h"
#include "syscalls.h"
#include "join.h"
#include "fat16.h"
#include "format.h"

void thread() {
    uart_printf("thread started\n");
    char* data;
    recv(true, 0, NULL, NULL, (uint64_t*) &data, NULL);
    uart_printf("got: %s\n", data);
    sleep(5, 0);
    uart_printf("this should never be reached\n");
    while (1);
}

void _start() {
    asm volatile(
        ".option push\n"
        ".option norelax\n"
        "lla gp, __global_pointer$\n"
        ".option pop\n"
    );

    uart_printf("uwud started\n");
    capability_t cap;
    spawn_thread(thread, NULL, 0, &cap);
    uart_printf("spawned thread\n");
    send(true, cap, MSG_TYPE_DATA, (uint64_t) "from uwud", 10);
    uart_printf("message sent\n");
    send(true, cap, 4, 69, 0);
    uint64_t exit_code;
    recv(true, cap, NULL, NULL, &exit_code, NULL);
    uart_printf("killed thread with exit code %i\n", exit_code);

    while(1);
}

