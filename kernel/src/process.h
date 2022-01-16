#ifndef PROCESS_H
#define PROCESS_H

#include <stdbool.h>
#include <stdint.h>

#include "elf.h"
#include "interrupt.h"
#include "mmu.h"
#include "time.h"

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

    process_message_t* message_queue;
    size_t message_queue_start;
    size_t message_queue_end;
    size_t message_queue_len;
    size_t message_queue_cap;
} process_t;

// init_processes() -> void
// Initialises process related stuff.
void init_processes();

// spawn_process_from_elf(pid_t, elf_t*, size_t, void*, size_t) -> pid_t
// Spawns a process using the given elf file and parent pid. Returns -1 on failure.
pid_t spawn_process_from_elf(pid_t parent_pid, elf_t* elf, size_t stack_size, void* args, size_t arg_size);

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

// enqueue_message_to_process(pid_t, process_message_t) -> bool
// Enqueues a message to a process's message queue. Returns true if successful and false if the process was not found or if the queue is full.
bool enqueue_message_to_process(pid_t recipient, process_message_t message);

// dequeue_message_from_process(pid_t, process_message_t) -> bool
// Dequeues a message from a process's message queue. Returns true if successful and false if the process was not found or if the queue is empty.
bool dequeue_message_from_process(pid_t pid, process_message_t* message);

#endif /* PROCESS_H */
