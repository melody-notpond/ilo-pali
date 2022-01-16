#include "syscall.h"

void uart_write(char* s, size_t length) {
    syscall(0, (uint64_t) s, length, 0, 0, 0, 0);
}

// alloc_page(void* addr, size_t count, int permissions) -> void* addr
void* alloc_page(void* addr, size_t count, int permissions) {
    return (void*) syscall(1, (uint64_t) addr, count, permissions, 0, 0, 0).first;
}

int page_permissions(void* addr, size_t count, int permissions) {
    return syscall(2, (uint64_t) addr, count, permissions, 0, 0, 0).first;
}

int dealloc_page(void* addr, size_t count) {
    return syscall(3, (uint64_t) addr, count, 0, 0, 0, 0).first;
}

time_t sleep(uint64_t seconds, uint64_t nanos) {
    dual_t dual = syscall(7, seconds, nanos, 0, 0, 0, 0);
    return (time_t) {
        .seconds = dual.first,
        .nanos = dual.second,
    };
}


                    // recv(bool block, pid_t* pid, int* type, uint64_t* data, uint64_t* metadata) -> int status
int recv(bool block, uint64_t* pid, int* type, uint64_t* data, uint64_t* metadata) {
    return syscall(11, block, (uint64_t) pid, (uint64_t) type, (uint64_t) data, (uint64_t) metadata, 0).first;
}
