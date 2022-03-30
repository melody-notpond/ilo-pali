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
    exit(0);
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

    while(1);
}

