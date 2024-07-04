#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// uart_puts(char*) -> void
// Prints out a message onto the UART.
void uart_puts(char* msg);

// // uart_puts(char*) -> void
// // Prints out a message onto the UART.
// void uart_puts(char* msg);

// #define PAGE_PERM_READ  4
// #define PAGE_PERM_WRITE 2
// #define PAGE_PERM_EXEC  1

// // page_alloc(size_t page_count, int permissions) -> void*
// // Allocates a page with the given permissions.
// void* page_alloc(size_t page_count, int permissions);

// // page_perms(void* page, size_t page_count, int permissions) -> int
// // Changes the page's permissions. Returns 0 if successful and 1 if not.
// int page_perms(void* page, size_t page_count, int permissions);

// // page_dealloc(void* page, size_t page_count) -> int
// // Deallocates a page. Returns 0 if successful and 1 if not.
// int page_dealloc(void* page, size_t page_count);

// // sleep(uint64_t seconds, uint64_t micros) -> void
// // Sleeps for the given amount of time.
// void sleep(uint64_t seconds, uint64_t micros);

// typedef uint64_t pid_t;

// // spawn(void* elf, size_t elf_size, char* name, size_t argc, char** argv) -> pid_t
// // Spawns a new process. Returns -1 on error.
// pid_t spawn(void* elf, size_t elf_size, char* name, size_t argc, char** argv);

// // spawn_thread(void (*func)(void* data), void* data) -> pid_t
// // Spawns a new process in the same address space, executing the given function.
// pid_t spawn_thread(void (*func)(void* data), void* data);

// // exit(int64_t code) -> !
// // Exits the current process.
// __attribute__((noreturn))
// void exit(int64_t code);

// struct allowed_memory {
//      char name[16];
//      void* start;
//      size_t size;
// };

// // get_allowed_memory(size_t i, struct allowed_memory* memory) -> bool
// // Gets an element of the allowed memory list. Returns true if the given index exists and false if out of bounds.
// //
// // The struct is defined below:
// // struct allowed_memory {
// //      char name[16];
// //      void* start;
// //      size_t size;
// // };
// bool get_allowed_memory(size_t i, struct allowed_memory* memory);

// // map_physical_memory(void* start, size_t size, int perms) -> void*
// // Maps a given physical range of memory to a virtual address for usage by the process. The process must have the ability to use this memory range or else it will return NULL.
// void* map_physical_memory(void* start, size_t size, int perms);

// // set_fault_handler(void (*handler)(int cause, uint64_t pc, uint64_t sp, uint64_t fp)) -> void
// // Sets the fault handler for the current process.
// void set_fault_handler(void (*handler)(int cause, uint64_t pc, uint64_t sp, uint64_t fp));

// #define LOCK_WAIT 0
// #define LOCK_WAKE 1
// #define LOCK_U8   0
// #define LOCK_U16  2
// #define LOCK_U32  4
// #define LOCK_U64  6

// // lock(void* ref, int type, uint64_t value) -> int status
// // Locks the current process until the given condition is true. Returns 0 on success.
// // Types:
// // - WAIT    - 0
// //      Waits while the pointer provided is the same as value.
// // - WAKE    - 1
// //      Wakes when the pointer provided is the same as value.
// // - SIZE     - 0,2,4,6
// //      Determines the size of the value. 0 = u8, 6 = u64
// int lock(void* ref, int type, uint64_t value);

// // capability_data(size_t* index, char* name, uint64_t* data_top, uint64_t* data_bot) -> int type
// // Gets the data of the capability associated with the given index. Returns the type of the capability.
// // This is useful for doing things like `switch (capability_data(&index, ...)) { ... }`.
// // The buffer must be at least 16 characters long.
// // Types:
// // - NONE      - 0
// //      No capability is associated with this index.
// // - CHANNEL   - 1
// //      The capability is a channel.
// // - MEMORY    - 2
// //      The capability is an ability to use a range of physical memory.
// // - INTERRUPT - 3
// //      The capability is an ability to process an interrupt.
// // - KILL      - 4
// //      The capability is an ability to kll another process.
// int capability_data(size_t* index, char* name, uint64_t* data_top, uint64_t* data_bot);

#endif /* SYSCALL_H */
