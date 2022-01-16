#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t first;
    uint64_t second;
} dual_t;

// syscall(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> dual_t
// Performs a syscall.
dual_t syscall(uint64_t syscall, uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);


void uart_write(char* s, size_t length);

#define PERM_READ   0x04
#define PERM_WRITE  0x02
#define PERM_EXEC   0x01

// alloc_page(void* addr, size_t count, int permissions) -> void* addr
void* alloc_page(void* addr, size_t count, int permissions);

int page_permissions(void* addr, size_t count, int permissions);

int dealloc_page(void* addr, size_t count);

typedef struct {
    uint64_t seconds;
    uint64_t nanos;
} time_t;

time_t sleep(uint64_t seconds, uint64_t nanos);

uint64_t spawn_process(void* exe, size_t exe_size, void* args, size_t arg_size);

uint64_t kill(uint64_t pid);

int send(bool block, uint64_t pid, int type, uint64_t data, uint64_t metadata);

void lock(void* ref, int type, uint64_t value);

uint64_t spawn_thread(void (*func)(void*, size_t), void* args, size_t arg_size);

#endif /* SYSCALL_H */
