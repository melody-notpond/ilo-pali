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

    void* fault_handler;
    bool faulted;
    void* fault_stack;

    process_state_t state;
    mmu_level_1_t* mmu_data;
    time_t wake_on_time;
    void* lock_ref;
    uint64_t lock_value;
    int lock_type;

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

#endif /* PROCESS_H */
