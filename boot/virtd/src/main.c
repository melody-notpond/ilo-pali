#include "syscalls.h"

void _start() {
    uart_write("spawned virtd!\n", 15);

    send(true, 0, MSG_TYPE_SIGNAL, 0, 0);

    kill(getpid());
}
