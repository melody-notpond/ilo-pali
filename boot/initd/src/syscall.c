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

uint64_t spawn_process(void* exe, size_t exe_size, void* args, size_t arg_size) {
    return syscall(8, (uint64_t) exe, exe_size, (uint64_t) args, arg_size, 0, 0).first;
}

uint64_t kill(uint64_t pid) {
    return syscall(9, pid, 0, 0, 0, 0, 0).first;
}

int send(bool block, uint64_t pid, int type, uint64_t data, uint64_t metadata) {
    return syscall(10, block, pid, type, data, metadata, 0).first;
}

void lock(void* ref, int type, uint64_t value) {
    syscall(12, (uint64_t) ref, type, value, 0, 0, 0);
}
