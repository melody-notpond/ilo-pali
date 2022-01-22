#include "syscalls.h"
#include "join.h"
#include "virtio.h"

void _start(void* args, size_t arg_size, uint64_t cap_high, uint64_t cap_low) {
    uart_printf("spawned virtd!\n");

    capability_t cap = ((capability_t) cap_high) << 64 | (capability_t) cap_low;
    uint64_t* args64 = args;
    send(true, &cap, MSG_TYPE_SIGNAL, 0, args64[0]);
    send(true, &cap, MSG_TYPE_SIGNAL, 1, args64[1] / PAGE_SIZE);

    pid_t pid;
    int type;
    uint64_t data;
    uint64_t meta;
    recv(true, &cap, &pid, &type, &data, &meta);

    volatile virtio_mmio_t* mmio = (void*) data;
    if (mmio->magic_value != 0x74726976)
        *(volatile char*) NULL = 0;
    if (mmio->device_id != 0) {
        uart_printf("device with id %x found!\n", mmio->device_id);
        kill(getpid());
    } else {
        uart_printf("no device\n");
    }

    while(1);
}
