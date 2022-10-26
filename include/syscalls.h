#ifndef SYSCALL_H
#define SYSCALL_H

#include <stddef.h>
#include <stdint.h>

// uart_puts(char*) -> void
// Prints out a message onto the UART.
void uart_puts(char* msg);

// uart_puts(char*) -> void
// Prints out a message onto the UART.
void uart_puts(char* msg);

#define PAGE_PERM_READ  4
#define PAGE_PERM_WRITE 2
#define PAGE_PERM_EXEC  1

// page_alloc(size_t page_count, int permissions) -> void*
// Allocates a page with the given permissions.
void* page_alloc(size_t page_count, int permissions);

// page_perms(void* page, size_t page_count, int permissions) -> int
// Changes the page's permissions. Returns 0 if successful and 1 if not.
int page_perms(void* page, size_t page_count, int permissions);

// page_dealloc(void* page, size_t page_count) -> int
// Deallocates a page. Returns 0 if successful and 1 if not.
int page_dealloc(void* page, size_t page_count);

// sleep(uint64_t seconds, uint64_t micros) -> void
// Sleeps for the given amount of time.
void sleep(uint64_t seconds, uint64_t micros);

typedef uint64_t pid_t;
// spawn_thread(void (*func)(void* data), void* data) -> pid_t
// Spawns a new process in the same address space, executing the given function.
pid_t spawn_thread(void (*func)(void* data), void* data);

// exit(int64_t code) -> !
// Exits the current process.
__attribute__((noreturn))
void exit(int64_t code);

#endif /* SYSCALL_H */
