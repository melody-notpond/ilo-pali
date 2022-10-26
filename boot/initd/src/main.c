#include "syscalls.h"

void say(void* msg) {
    char* message = msg;
    for (int i = 0; i < 3; i++) {
        uart_puts(message);
        sleep(1, 0);
    }
    exit(0);
}

int _start() {
    uart_puts("hewo!");
    spawn_thread(say, "uwu");
    sleep(0, 500000);
    spawn_thread(say, "owo");
    while(1);
}

