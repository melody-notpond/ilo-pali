#include "syscalls.h"
#include "join.h"
#include "virtio.h"

void _start(void* args, size_t arg_size, uint64_t cap_high, uint64_t cap_low) {
    uart_printf("spawned virtd!\n");

    capability_t cap = ((capability_t) cap_high) << 64 | (capability_t) cap_low;
    uint64_t* args64 = args;
    send(true, &cap, MSG_TYPE_SIGNAL, 1, args64[0]);
    send(true, &cap, MSG_TYPE_SIGNAL, 2, args64[1] / PAGE_SIZE);

    pid_t pid;
    int type;
    uint64_t data;
    uint64_t meta;
    recv(true, &cap, &pid, &type, &data, &meta);
    uart_printf("%p\n", (void*) data);
    //virtio_init_mmio(NULL, 0, NULL, NULL);

    while(1);
}
