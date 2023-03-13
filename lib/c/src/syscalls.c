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

// spawn(void* elf, size_t elf_size, char* name, size_t argc, char** argv) -> pid_t
// Spawns a new process. Returns -1 on error.
pid_t spawn(void* elf, size_t elf_size, char* name, size_t argc, char** argv) {
    return syscall(5, (intptr_t) elf, elf_size, (intptr_t) name, argc, (intptr_t) argv, 0);
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

// set_fault_handler(void (*handler)(int cause, uint64_t pc, uint64_t sp, uint64_t fp)) -> void
// Sets the fault handler for the current process.
void set_fault_handler(void (*handler)(int cause, uint64_t pc, uint64_t sp, uint64_t fp)) {
    syscall(8, (intptr_t) handler, 0, 0, 0, 0, 0);
}

// lock(void* ref, int type, uint64_t value) -> int status
// Locks the current process until the given condition is true. Returns 0 on success.
// Types:
// - WAIT    - 0
//      Waits while the pointer provided is the same as value.
// - WAKE    - 1
//      Wakes when the pointer provided is the same as value.
// - SIZE     - 0,2,4,6
//      Determines the size of the value. 0 = u8, 6 = u64
int lock(void* ref, int type, uint64_t value) {
    return syscall(9, (intptr_t) ref, type, value, 0, 0, 0);
}


// capability_data(size_t* index, char* name, uint64_t* data_top, uint64_t* data_bot) -> int type
// Gets the data of the capability associated with the given index. Returns the type of the capability.
// This is useful for doing things like `switch (capability_data(&index, ...)) { ... }`.
// The buffer must be at least 16 characters long.
// Types:
// - NONE      - 0
//      No capability is associated with this index.
// - CHANNEL   - 1
//      The capability is a channel.
// - MEMORY    - 2
//      The capability is an ability to use a range of physical memory.
// - INTERRUPT - 3
//      The capability is an ability to process an interrupt.
// - KILL      - 4
//      The capability is an ability to kll another process.
int capability_data(size_t* index, char* name, uint64_t* data_top, uint64_t* data_bot) {
    return syscall(10, (intptr_t) index, (intptr_t) name, (intptr_t) data_top, (intptr_t) data_bot, 0, 0);
}