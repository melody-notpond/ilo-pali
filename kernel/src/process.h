#ifndef PROCESS_H
#define PROCESS_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "elf.h"
#include "interrupt.h"
#include "mmu.h"
#include "time.h"

#define PROCESS_MESSAGE_QUEUE_SIZE  128
#define PROCESS_NAME_SIZE           255

typedef uint64_t capability_t;
typedef uint64_t pid_t;

typedef struct {
    pid_t source;
    uint64_t type;
    uint64_t data;
    uint64_t metadata;
} process_message_t;

typedef struct s_process_channel {
    size_t start;
    size_t end;
    size_t len;
    pid_t recipient_pid;
    capability_t recipient_channel;
    struct s_process_channel* recipient;

    struct {
        uint8_t user : 1;
        uint8_t integer : 1;
        uint8_t pointer : 1;
        uint8_t data : 1;
        uint8_t capability : 1;
        struct {
            uint8_t kill : 1;
            uint8_t murder : 2;
            uint8_t interrupt : 2;
            uint8_t suspend : 2;
            uint8_t resume : 2;
            uint8_t segfault : 1;
        } kill;
        struct {
            uint8_t killed : 1;
            uint8_t murdered : 1;
            uint8_t interrupted : 1;
            uint8_t suspended : 1;
            uint8_t resumed : 1;
            uint8_t segfaulted : 1;
            uint8_t normal : 1;
        } recipient_killed;
    } allowed;

    process_message_t message_queue[PROCESS_MESSAGE_QUEUE_SIZE];
} process_channel_t;

typedef enum {
    PROCESS_STATE_DEAD = 0,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCK_SLEEP,
    PROCESS_STATE_BLOCK_LOCK,
    PROCESS_STATE_WAIT,
} process_state_t;

typedef struct {
    char* name;
    pid_t pid;
    pid_t thread_source;
    atomic_bool mutex_lock;

    process_state_t state;
    mmu_level_1_t* mmu_data;
    time_t wake_on_time;
    void* lock_ref;
    uint64_t lock_value;
    int lock_type;

    process_channel_t** channels;
    size_t channels_len;
    size_t channels_cap;

    void* last_virtual_page;

    uint64_t pc;
    uint64_t xs[32];
    double fs[32];
} process_t;

// init_processes() -> void
// Initialises process related stuff.
void init_processes();

// get_process(pid_t) -> process_t*
// Gets the process associated with the pid.
process_t* get_process(pid_t pid);

// get_process(pid_t) -> bool
// Checks if the given pid has an associated process.
bool process_exists(pid_t pid);

// unlock_process(process_t*) -> void
// Unlocks the mutex associated with the process and frees a process hashmap reader.
void unlock_process(process_t* process);

// spawn_process_from_elf(char*, size_t, elf_t*, size_t, void*, size_t) -> process_t*
// Spawns a process using the given elf file. Returns NULL on failure.
process_t* spawn_process_from_elf(char* name, size_t name_size, elf_t* elf, size_t stack_size, void* args, size_t arg_size);

// spawn_thread_from_func(pid_t, void*, size_t, void*, size_t) -> process_t*
// Spawns a thread from the given process. Returns NULL on failure.
process_t* spawn_thread_from_func(pid_t parent_pid, void* func, size_t stack_size, void* args, size_t arg_size);

// create_capability(capability_t*, process_t*, capability_t*, process_t*, bool, bool) -> void
// Creates a capability pair. The provided pointers are set to the capabilities.
void create_capability(capability_t* a, process_t* process_a, capability_t* b, process_t* process_b, bool fa, bool fb);

// save_process(trap_t*) -> void
// Saves a process and pushes it onto the queue.
void save_process(trap_t* trap);

// switch_to_process(pid_t) -> void
// Jumps to the given process.
void switch_to_process(trap_t* trap, pid_t pid);

// get_next_waiting_process(pid_t) -> pid_t
// Searches for the next waiting process. Returns -1 if not found.
pid_t get_next_waiting_process(pid_t pid);

// kill_process(pid_t, bool) -> void
// Kills a process.
void kill_process(pid_t pid, bool erase);

// transfer_capability(capability_t, pid_t, pid_t) -> capability_t
// Transfers the given capability to a new process. Returns the capability if successful and -1 if not.
capability_t transfer_capability(capability_t capability, pid_t old_owner, pid_t new_owner);

// clone_capability(pid_t, capability_t, capability_t*) -> int
// Clones a capability. Returns 0 on success and 1 if the pid doesn't match or the capability is invalid.
int clone_capability(pid_t pid, capability_t original, capability_t* new);

// enqueue_message_to_channel(capability_t, pid_t, process_message_t) -> int
// Enqueues a message to a channel's message queue. Returns 0 if successful, 1 if the capability is invalid, 2 if the queue is full, and 3 if the connection has closed.
int enqueue_message_to_channel(capability_t capability, pid_t pid, process_message_t message);

// enqueue_interrupt_to_channel(capability_t, pid_t, uint32_t) -> int
// Enqueues an interrupt to a channel's message queue. Returns 0 if successful, 1 if the capability is invalid, 2 if the queue is full, and 3 if the connection has closed.
int enqueue_interrupt_to_channel(capability_t capability, pid_t pid, uint32_t id);

// dequeue_message_from_channel(pid_t, capability_t, process_message_t*) -> int
// Dequeues a message from a channel's message queue. Returns 0 if successful, 1 if the capability is invalid, 2 if the queue is empty, and 3 if the channel has closed.
int dequeue_message_from_channel(pid_t pid, capability_t capability, process_message_t* message);

// capability_connects_to_initd(capability_t, pid_t) -> bool
// Returns true if the capability connects to initd or one of its threads.
bool capability_connects_to_initd(capability_t capability, pid_t pid);

#endif /* PROCESS_H */
