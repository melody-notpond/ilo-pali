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

typedef enum {
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCK_SLEEP,
    PROCESS_STATE_BLOCK_LOCK,
    PROCESS_STATE_WAIT,
} process_state_t;

struct allowed_memory {
     char name[16];
     void* start;
     size_t size;
};

#define PROCESS_MAX_ALLOWED_MEMORY_RANGES 16
#define CAPABILITIES_MAX_ALLOWED          1024
#define CAPABILITIES_QUEUE_SIZE           1024

typedef enum {
    MESSAGE_TYPE_INTEGER     = 0,
    MESSAGE_TYPE_USER_SIGNAL = 1,
    MESSAGE_TYPE_DATA        = 2,
    MESSAGE_TYPE_POINTER     = 3,
    MESSAGE_TYPE_INTERRUPT   = 4,
    MESSAGE_TYPE_CHILD_DEATH = 5,
    MESSAGE_TYPE_KILL_SIGNAL = 6,
} message_type_t;

typedef enum {
    MESSAGE_KILL_TYPE_KILL        = 0,
    MESSAGE_KILL_TYPE_MURDER      = 1,
    MESSAGE_KILL_TYPE_INTERRUPT   = 2,
    MESSAGE_KILL_TYPE_SUSPEND     = 3,
    MESSAGE_KILL_TYPE_RESUME      = 4,
    MESSAGE_KILL_TYPE_SEGFAULT    = 5,
    MESSAGE_KILL_TYPE_NORMAL_EXIT = 6,
} message_kill_type_t;

typedef struct {
    pid_t transmitter;
    message_type_t type;
    uint64_t metadata;
    uint64_t data;
} channel_message_t;

typedef struct {
    char name[16];

    enum {
        CAPABILITY_INTERNAL_TYPE_NIL,
        CAPABILITY_INTERNAL_TYPE_CHANNEL,
        CAPABILITY_INTERNAL_TYPE_MEMORY_RANGE,
        CAPABILITY_INTERNAL_TYPE_INTERRUPT,
        CAPABILITY_INTERNAL_TYPE_KILL,
    } type;

    union {
        struct {
            pid_t target;
            uint8_t user : 1;
            uint8_t integer : 1;
            uint8_t pointer : 1;
            uint8_t data : 1;
        } channel;

        struct {
            intptr_t start;
            intptr_t end;
        } memory_range;

        struct {
            uint64_t interrupt_mask_1;
            uint64_t interrupt_mask_2;
        } interrupt;

        struct {
            pid_t target;
            uint8_t kill : 1;
            uint8_t murder : 2;
            uint8_t interrupt : 2;
            uint8_t suspend : 2;
            uint8_t resume : 2;
            uint8_t segfault : 1;
        } kill;
    } data;
} capability_internal_t;

/*
typedef struct s_channel {
    struct s_channel* recipient;
    size_t queue_length;
    size_t queue_start;
    size_t queue_end;

    struct {
        uint8_t user : 1;
        uint8_t integer : 1;
        uint8_t pointer : 1;
        uint8_t data : 1;
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
            uint8_t normal_exit : 1;
        } recipient_killed;
    } allowed;

    channel_message_t message_queue[CAPABILITIES_QUEUE_SIZE];
} channel_t;
*/

typedef struct {
    char* name;
    pid_t pid;
    pid_t thread_source;
    atomic_bool mutex_lock;

    void* fault_handler;
    bool faulted;
    void* fault_stack;

    process_state_t state;
    struct mmu_root mmu_data;
    time_t wake_on_time;
    void* lock_ref;
    uint64_t lock_value;
    int lock_type;

    /*
    size_t channels_len;
    size_t channels_cap;
    channel_t* channels;
    */

    size_t capabilities_len;
    size_t capabilities_cap;
    capability_internal_t* capabilities;

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

// get_process_unsafe(pid_t) -> process_t*
// Gets the process associated with the pid without checking for mutex lock.
process_t* get_process_unsafe(pid_t pid);

// get_process(pid_t) -> bool
// Checks if the given pid has an associated process.
bool process_exists(pid_t pid);

// unlock_process(process_t*) -> void
// Unlocks the mutex associated with the process and frees a process hashmap reader.
void unlock_process(process_t* process);

// spawn_process_from_elf(char*, size_t, elf_t*, size_t, size_t, char**) -> process_t*
// Spawns a process using the given elf file. Returns NULL on failure.
process_t* spawn_process_from_elf(char* name, size_t name_size, elf_t* elf, size_t stack_size, size_t argc, char** args);

// spawn_thread_from_func(pid_t, void*, size_t, void*) -> process_t*
// Spawns a thread from the given process. Returns NULL on failure.
process_t* spawn_thread_from_func(pid_t parent_pid, void* func, size_t stack_size, void* args);

// save_process(trap_t*) -> void
// Saves a process and pushes it onto the queue.
void save_process(trap_t* trap);

// switch_to_process(pid_t) -> void
// Jumps to the given process.
void switch_to_process(trap_t* trap, pid_t pid);

// get_next_waiting_process(pid_t) -> pid_t
// Searches for the next waiting process. Returns -1 if not found.
pid_t get_next_waiting_process(pid_t pid);

// push_capability(pid_t, capability_internal_t) -> void
// Pushes a capability to a process's list of capabilities.
void push_capability(pid_t pid, capability_internal_t cap);

// kill_process(pid_t) -> void
// Kills a process.
void kill_process(pid_t pid);

#endif /* PROCESS_H */
