#include "syscalls.h"

void _start(void* _args, size_t _size, uint64_t cap_high, uint64_t cap_low) {
    uart_write("spawned virtd!\n", 15);

    capability_t cap = ((capability_t) cap_high) << 64 | (capability_t) cap_low;
    send(true, &cap, MSG_TYPE_INT, 69, 420);

    while(1);
}
