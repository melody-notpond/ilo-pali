#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#include "elf.h"
#include "interrupt.h"
#include "mmu.h"

typedef uint64_t pid_t;
typedef uint64_t uid_t;

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

    void* last_virtual_page;

    uint64_t pc;
    uint64_t xs[32];
    double fs[32];
} process_t;

// init_processes() -> void
// Initialises process related stuff.
void init_processes();

// spawn_process_from_elf(pid_t, elf_t*, size_t) -> pid_t
// Spawns a process using the given elf file and parent pid. Returns -1 on failure.
pid_t spawn_process_from_elf(pid_t parent_pid, elf_t* elf, size_t stack_size);

// switch_to_process(pid_t) -> void
// Jumps to the given process.
void switch_to_process(trap_t* trap, pid_t pid);

// get_next_waiting_process(pid_t) -> pid_t
// Searches for the next waiting process. Returns the given pid if not found.
pid_t get_next_waiting_process(pid_t pid);

// get_process(pid_t) -> process_t*
// Gets the process associated with the pid.
process_t* get_process(pid_t pid);

#endif /* PROCESS_H */
