#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#include "elf.h"
#include "interrupt.h"

typedef uint64_t pid_t;

// init_processes() -> void
// Initialises process related stuff.
void init_processes();

// spawn_process_from_elf(pid_t, elf_t*, size_t) -> pid_t
// Spawns a process using the given elf file and parent pid. Returns -1 on failure.
pid_t spawn_process_from_elf(pid_t parent_pid, elf_t* elf, size_t stack_size);

// switch_to_process(pid_t) -> void
// Jumps to the given process.
void switch_to_process(trap_t* trap, pid_t pid);

#endif /* PROCESS_H */
