#ifndef PROCESS_H
#define PROCESS_H

#include <stdbool.h>
#include <stdint.h>

#include "elf.h"
#include "interrupt.h"
#include "mmu.h"
#include "time.h"

#define PROCESS_MESSAGE_QUEUE_SIZE 128

typedef __uint128_t capability_t;
typedef uint64_t pid_t;
typedef uint64_t uid_t;

typedef struct {
    pid_t source;
    uint64_t type;
    uint64_t data;
    uint64_t metadata;
} process_message_t;

typedef enum {
    PROCESS_STATE_DEAD = 0,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCK_SLEEP,
    PROCESS_STATE_BLOCK_LOCK,
    PROCESS_STATE_WAIT,
} process_state_t;

typedef struct {
    pid_t pid;
    pid_t thread_source;

    uid_t user;

    process_state_t state;
    mmu_level_1_t* mmu_data;
    time_t wake_on_time;
    void* lock_ref;
    uint64_t lock_value;
    int lock_type;

    void* last_virtual_page;

    uint64_t pc;
    uint64_t xs[32];
    double fs[32];
} process_t;

typedef struct {
    process_message_t* message_queue;
    size_t start;
    size_t end;
    size_t len;
    capability_t sender;
    pid_t receiver;
} process_channel_t;

// init_processes() -> void
// Initialises process related stuff.
void init_processes();

// spawn_process_from_elf(pid_t, elf_t*, size_t, void*, size_t) -> pid_t
// Spawns a process using the given elf file and parent pid. Returns -1 on failure.
pid_t spawn_process_from_elf(pid_t parent_pid, elf_t* elf, size_t stack_size, void* args, size_t arg_size);

// spawn_thread_from_func(pid_t, void*, size_t, void*, size_t) -> pid_t
// Spawns a thread from the given process. Returns -1 on failure.
pid_t spawn_thread_from_func(pid_t parent_pid, void* func, size_t stack_size, void* args, size_t arg_size);

// create_capability(capability_t*, pid_t, capability_t*, pid_t) -> void
// Creates a capability pair. The provided pointers are set to the capabilities.
void create_capability(capability_t* a, pid_t pa, capability_t* b, pid_t pb);

// switch_to_process(pid_t) -> void
// Jumps to the given process.
void switch_to_process(trap_t* trap, pid_t pid);

// get_next_waiting_process(pid_t) -> pid_t
// Searches for the next waiting process. Returns the given pid if not found.
pid_t get_next_waiting_process(pid_t pid);

// get_process(pid_t) -> process_t*
// Gets the process associated with the pid.
process_t* get_process(pid_t pid);

// kill_process(pid_t) -> void
// Kills a process.
void kill_process(pid_t pid);

// transfer_capability(capability_t, pid_t, pid_t) -> int
// Transfers the given capability to a new process. Returns 0 if successful, 1 if the capability is invalid, and 2 if the new owner doesn't exist.
int transfer_capability(capability_t capability, pid_t old_owner, pid_t new_owner);

// enqueue_message_to_channel(capability_t, process_message_t) -> int
// Enqueues a message to a channel's message queue. Returns 0 if successful, 1 if the capability is invalid, 2 if the queue is full, and 3 if the connection has closed.
int enqueue_message_to_channel(capability_t capability, process_message_t message);

// enqueue_interrupt_to_channel(capability_t, uint32_t) -> int
// Enqueues an interrupt to a channel's message queue. Returns 0 if successful, 1 if the capability is invalid, 2 if the queue is full, and 3 if the connection has closed.
int enqueue_interrupt_to_channel(capability_t capability, uint32_t id);

// dequeue_message_from_channel(pid_t, capability_t, process_message_t*) -> int
// Dequeues a message from a channel's message queue. Returns 0 if successful, 1 if the capability is invalid, 2 if the queue is empty, and 3 if the channel has closed.
int dequeue_message_from_channel(pid_t pid, capability_t capability, process_message_t* message);

// capability_connects_to_initd(capability_t) -> bool
// Returns true if the capability connects to initd or one of its threads.
bool capability_connects_to_initd(capability_t capability);

#endif /* PROCESS_H */
