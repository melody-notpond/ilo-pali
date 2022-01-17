#include "syscalls.h"

typedef struct {
    uint64_t first;
    uint64_t second;
} dual_t;

// syscall(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> dual_t
// Performs a syscall.
dual_t syscall(uint64_t syscall, uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);

// uart_write(char* data, size_t length) -> void
// Writes to the uart port.
void uart_write(char* s, size_t length) {
    syscall(0, (uint64_t) s, length, 0, 0, 0, 0);
}

// alloc_page(void* addr, size_t count, int permissions) -> void* addr
// Allocates `count` pages of memory containing addr. If addr is NULL, then it allocates the next available page. Returns NULL on failure. Write and execute cannot both be set at the same time.
void* alloc_page(void* addr, size_t count, int permissions) {
    return (void*) syscall(1, (uint64_t) addr, count, permissions, 0, 0, 0).first;
}

// page_permissions(void* addr, size_t count, int permissions) -> int status
// Modifies the permissions of the given pages. Returns 0 on success, 1 if the page was never allocated, and 2 if invalid permissions.
//
// Permissions:
// - READ    - 0b100
// - WRITE   - 0b010
// - EXECUTE - 0b001
int page_permissions(void* addr, size_t count, int permissions) {
    return syscall(2, (uint64_t) addr, count, permissions, 0, 0, 0).first;
}

// dealloc_page(void* addr, size_t count) -> int status
// Deallocates the page(s) containing the given address. Returns 0 on success and 1 if a page was never allocated by this process.
int dealloc_page(void* addr, size_t count) {
    return syscall(3, (uint64_t) addr, count, 0, 0, 0, 0).first;
}

// getpid() -> pid_t
// Gets the pid of the current process.
pid_t getpid() {
    return syscall(4, 0, 0, 0, 0, 0, 0).first;
}

// getuid(pid_t pid) -> uid_t
// Gets the uid of the given process. Returns -1 if the process doesn't exist.
uid_t getuid(pid_t pid) {
    return syscall(5, pid, 0, 0, 0, 0, 0).first;
}

// setuid(pid_t pid, uid_t uid) -> int status
// Sets the uid of the given process (can only be done by processes with uid = 0). Returns 0 on success, 1 if the process does not exist, and 2 if insufficient permissions.
int setuid(pid_t pid, uid_t uid) {
    return syscall(6, pid, uid, 0, 0, 0, 0).first;
}

// sleep(size_t seconds, size_t micros) -> time_t current
// Sleeps for the given amount of time. Returns the current time. Does not interrupt receive handlers or interrupt handlers. If the sleep time passed in is 0, then the syscall returns immediately.
time_t sleep(uint64_t seconds, uint64_t micros) {
    dual_t dual = syscall(7, seconds, micros, 0, 0, 0, 0);
    return (time_t) {
        .seconds = dual.first,
        .nanos = dual.second,
    };
}

// spawn(void* exe, size_t exe_size, void* args, size_t args_size) -> pid_t child
// Spawns a process with the given executable binary. Returns a pid of -1 on failure.
// The executable may be a valid elf file. All data will be copied over to a new set of pages.
uint64_t spawn_process(void* exe, size_t exe_size, void* args, size_t arg_size) {
    return syscall(8, (uint64_t) exe, exe_size, (uint64_t) args, arg_size, 0, 0).first;
}

// kill(pid_t pid) -> int status
// Kills the given process. Returns 0 on success, 1 if the process does not exist, and 2 if insufficient permissions.
uint64_t kill(uint64_t pid) {
    return syscall(9, pid, 0, 0, 0, 0, 0).first;
}

// send(bool block, pid_t pid, int type, uint64_t data, uint64_t metadata) -> int status
// Sends data to the given process. Returns 0 on success, 1 if process does not exist, 2 if invalid arguments, and 3 if message queue is full. Blocks until the message is sent if block is true. If block is false, then it immediately returns.
// Types:
// - SIGNAL  - 0
//      Metadata can be any integer argument for the signal (for example, the size of the requested data).
// - INT     - 1
//      Metadata can be set to send a 128 bit integer.
// - POINTER - 2
//      Metadata contains the size of the pointer. The kernel will share the pages necessary between processes.
// - DATA - 3
//      Metadata contains the size of the data. The kernel will copy the required data between processes. Maximum is 1 page.
int send(bool block, uint64_t pid, int type, uint64_t data, uint64_t metadata) {
    return syscall(10, block, pid, type, data, metadata, 0).first;
}

// recv(bool block, pid_t* pid, int* type, uint64_t* data, uint64_t* metadata) -> int status
// Blocks until a message is received and deposits the data into the pointers provided. If block is false, then it immediately returns. Returns 0 if message was received and 1 if not.
int recv(bool block, uint64_t* pid, int* type, uint64_t* data, uint64_t* metadata) {
    return syscall(11, block, (uint64_t) pid, (uint64_t) type, (uint64_t) data, (uint64_t) metadata, 0).first;
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
void lock(void* ref, int type, uint64_t value) {
    syscall(12, (uint64_t) ref, type, value, 0, 0, 0);
}

// spawn_thread(void (*func)(void*, size_t), void* data, size_t size) -> pid_t thread
// Spawns a thread (a process sharing the same memory as the current process) that executes the given function. Returns -1 on failure.
uint64_t spawn_thread(void (*func)(void*, size_t), void* args, size_t arg_size) {
    return syscall(13, (uint64_t) func, (uint64_t) args, arg_size, 0, 0, 0).first;
}

