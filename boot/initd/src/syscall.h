#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

// syscall(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> uint64_t
// Performs a syscall.
uint64_t syscall(uint64_t syscall, uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);

#endif /* SYSCALL_H */
