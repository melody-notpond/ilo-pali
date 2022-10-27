#include <stddef.h>
#include <stdint.h>

#include "syscalls.h"

uint64_t syscall(uint64_t call, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

// uart_puts(char*) -> void
// Prints out a message onto the UART.
void uart_puts(char* msg) {
    syscall(0, (uint64_t) msg, 0, 0, 0, 0, 0);
}

// page_alloc(size_t page_count, int permissions) -> void*
// Allocates a page with the given permissions.
void* page_alloc(size_t page_count, int permissions) {
    return (void*) syscall(1, page_count, permissions, 0, 0, 0, 0);
}

// page_perms(void* page, size_t page_count, int permissions) -> int
// Changes the page's permissions. Returns 0 if successful and 1 if not.
int page_perms(void* page, size_t page_count, int permissions) {
    return syscall(2, (intptr_t) page, page_count, permissions, 0, 0, 0);
}

// page_dealloc(void* page, size_t page_count) -> int
// Deallocates a page. Returns 0 if successful and 1 if not.
int page_dealloc(void* page, size_t page_count) {
    return syscall(3, (intptr_t) page,page_count, 0, 0, 0, 0);
}

// sleep(uint64_t seconds, uint64_t micros) -> void
// Sleeps for the given amount of time.
void sleep(uint64_t seconds, uint64_t micros) {
    syscall(4, seconds, micros, 0, 0, 0, 0);
}

// spawn_thread(void (*func)(void* data), void* data) -> pid_t
// Spawns a new process in the same address space, executing the given function.
pid_t spawn_thread(void (*func)(void* data), void* data) {
    return syscall(6, (intptr_t) func, (intptr_t) data, 0, 0, 0, 0);
}

// exit(int64_t code) -> !
// Exits the current process.
__attribute__((noreturn))
void exit(int64_t code) {
    syscall(7, code, 0, 0, 0, 0, 0);
    while(1);
}

// get_allowed_memory(size_t i, struct allowed_memory* memory) -> bool
// Gets an element of the allowed memory list. Returns true if the given index exists and false if out of bounds.
//
// The struct is defined below:
// struct allowed_memory {
//      char name[16];
//      void* start;
//      size_t size;
// };
bool get_allowed_memory(size_t i, struct allowed_memory* memory) {
    return syscall(8, i, (intptr_t) memory, 0, 0, 0, 0);
}

// map_physical_memory(void* start, size_t size, int perms) -> void*
// Maps a given physical range of memory to a virtual address for usage by the process. The process must have the ability to use this memory range or else it will return NULL.
void* map_physical_memory(void* start, size_t size, int perms) {
    return (void*) syscall(9, (intptr_t) start, size, perms, 0, 0, 0);
}
