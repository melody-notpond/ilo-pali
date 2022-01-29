#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint64_t pid_t;
typedef uint64_t uid_t;
typedef __uint128_t capability_t;

typedef struct {
    uint64_t seconds;
    uint64_t nanos;
} time_t;

typedef struct {
    void* virtual_;
    uint64_t physical;
} virtual_physical_pair_t;

// uart_write(char* data, size_t length) -> void
// Writes to the uart port.
void uart_write(char* s, size_t length);

#define PERM_READ   0x04
#define PERM_WRITE  0x02
#define PERM_EXEC   0x01

// alloc_page(size_t count, int permissions) -> void* addr
// Allocates `count` pages of memory. Returns NULL on failure. Write and execute cannot both be set at the same time.
void* alloc_page(size_t count, int permissions);

// page_permissions(void* addr, size_t count, int permissions) -> int status
// Modifies the permissions of the given pages. Returns 0 on success, 1 if the page was never allocated, and 2 if invalid permissions.
//
// Permissions:
// - READ    - 0b100
// - WRITE   - 0b010
// - EXECUTE - 0b001
int page_permissions(void* addr, size_t count, int permissions);

// dealloc_page(void* addr, size_t count) -> int status
// Deallocates the page(s) containing the given address. Returns 0 on success and 1 if a page was never allocated by this process.
int dealloc_page(void* addr, size_t count);

// getpid() -> pid_t
// Gets the pid of the current process.
pid_t getpid();

// getuid(pid_t pid) -> uid_t
// Gets the uid of the given process. Returns -1 if the process doesn't exist.
uid_t getuid(pid_t pid);

// setuid(pid_t pid, uid_t uid) -> int status
// Sets the uid of the given process (can only be done by processes with uid = 0). Returns 0 on success, 1 if the process does not exist, and 2 if insufficient permissions.
int setuid(pid_t pid, uid_t uid);

// sleep(size_t seconds, size_t micros) -> time_t current
// Sleeps for the given amount of time. Returns the current time. Does not interrupt receive handlers or interrupt handlers. If the sleep time passed in is 0, then the syscall returns immediately.
time_t sleep(uint64_t seconds, uint64_t micros);

// spawn(char*, size_t, void* exe, size_t exe_size, void* args, size_t args_size, capability_t* capability) -> pid_t child
// Spawns a process with the given executable binary. Returns a pid of -1 on failure.
// The executable may be a valid elf file. All data will be copied over to a new set of pages.
uint64_t spawn_process(char* name, size_t name_size, void* exe, size_t exe_size, void* args, size_t arg_size, capability_t* capability);

// kill(pid_t pid) -> int status
// Kills the given process. Returns 0 on success, 1 if the process does not exist, and 2 if insufficient permissions.
uint64_t kill(uint64_t pid);

#define MSG_TYPE_SIGNAL    0
#define MSG_TYPE_INT       1
#define MSG_TYPE_POINTER   2
#define MSG_TYPE_DATA      3
#define MSG_TYPE_INTERRUPT 4

// send(bool block, capability_t* channel, int type, uint64_t data, uint64_t metadata) -> int status
// Sends data to the given channel. Returns 0 on success, 1 if invalid arguments, and 2 if message queue is full. Blocks until the message is sent if block is true. If block is false, then it immediately returns. If an invalid capability is passed in, this kills the process.
// Types:
// - SIGNAL  - 0
//      Metadata can be any integer argument for the signal (for example, the size of the requested data).
// - INT     - 1
//      Metadata can be set to send a 128 bit integer.
// - POINTER - 2
//      Metadata contains the size of the pointer. The kernel will share the pages necessary between processes.
// - DATA - 3
//      Metadata contains the size of the data. The kernel will copy the required data between processes. Maximum is 1 page.
int send(bool block, capability_t* channel, int type, uint64_t data, uint64_t metadata);

// recv(bool block, capability_t* channel, pid_t* pid, int* type, uint64_t* data, uint64_t* metadata) -> int status
// Blocks until a message is received on the given channel and deposits the data into the pointers provided. If block is false, then it immediately returns. Returns 0 if message was received and 1 if not. Kills the process if an invalid capability was provided.
int recv(bool block, capability_t* channel, uint64_t* pid, int* type, uint64_t* data, uint64_t* metadata);

#define LOCK_WAIT   0
#define LOCK_WAKE   1
#define LOCK_U8     0
#define LOCK_U16    2
#define LOCK_U32    4
#define LOCK_U64    6

// lock(void* ref, int type, uint64_t value) -> int status
// Locks the current process until the given condition is true. Returns 0 on success.
// Types:
// - WAIT    - 0
//      Waits while the pointer provided is the same as value.
// - WAKE    - 1
//      Wakes when the pointer provided is the same as value.
// - SIZE     - 0,2,4,6
//      Determines the size of the value. 0 = u8, 6 = u64
void lock(void* ref, int type, uint64_t value);

// spawn_thread(void (*func)(void*, size_t, uint64_t, uint64_t), void* data, size_t size, capability_t*) -> pid_t thread
// Spawns a thread (a process sharing the same memory as the current process) that executes the given function. Returns -1 on failure.
uint64_t spawn_thread(void (*func)(void*, size_t, uint64_t, uint64_t), void* args, size_t arg_size, capability_t* capability);

// subscribe_to_interrupt(uint32_t id, capability_t* capability) -> void
// Subscribes to an interrupt.
void subscribe_to_interrupt(uint32_t id, capability_t* capability);

// alloc_pages_physical(void* addr, size_t count, int permissions, capability_t* capability) -> (void* virtual, intptr_t physical)
// Allocates `count` pages of memory that are guaranteed to be consecutive in physical memory. Returns (NULL, 0) on failure. Write and execute cannot both be set at the same time. If capability is NULL, the syscall returns failure. If the capability is invalid, the process is killed. If addr is not NULL, returns addresses that contain that address.
virtual_physical_pair_t alloc_pages_physical(void* addr, size_t count, int permissions, capability_t* capability);

// transfer_capability(capability_t* capability, pid_t pid) -> void
// Transfers the given capability to the process with the associated pid. Kills the process if the capability is invalid.
int transfer_capability(capability_t* capability, pid_t pid);

// clone_capability(capability_t*, capability_t*) -> void
// Clones a capability. If the capability is invalid, kills the process.
void clone_capability(capability_t* original, capability_t* new);

// create_capability(capability_t* cap1, capability_t* cap2) -> void
// Creates a pair of capabilities.
void create_capability(capability_t* cap1, capability_t* cap2);

#endif /* SYSCALL_H */
