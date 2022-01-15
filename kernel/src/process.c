#include "console.h"
#include "memory.h"
#include "process.h"

process_t* processes;
size_t processes_list_page_count;

pid_t MAX_PID = 1000;
pid_t current_pid = 0;

// init_processes() -> void
// Initialises process related stuff.
void init_processes() {
    processes_list_page_count = (MAX_PID * sizeof(process_t) + PAGE_SIZE - 1) / PAGE_SIZE;
    processes = alloc_pages(processes_list_page_count);
}

// spawn_process_from_elf(pid_t, elf_t*, size_t) -> pid_t
// Spawns a process using the given elf file and parent pid. Returns -1 on failure.
pid_t spawn_process_from_elf(pid_t parent_pid, elf_t* elf, size_t stack_size) {
    if (elf->header->type != ELF_EXECUTABLE) {
        console_puts("ELF file is not executable\n");
        return -1;
    }

    pid_t pid = -1;
    if (current_pid < MAX_PID) {
        pid = current_pid++;
    } else {
        for (size_t i = 0; i < MAX_PID; i++) {
            if (processes[i].state == PROCESS_STATE_DEAD) {
                pid = i;
                break;
            }
        }

        if (pid == (uint64_t) -1)
            return -1;
    }

    mmu_level_1_t* top;
    if (current_pid == 1) {
        top = get_mmu();
    } else {
        top = create_mmu_table();
        identity_map_kernel(top, NULL, NULL, NULL);
        for (size_t i = 0; i < processes_list_page_count; i++) {
            void* p = (void*) processes + i * PAGE_SIZE;
            mmu_map(top, p, p, MMU_BIT_READ | MMU_BIT_WRITE);
        }
    }

    page_t* max_page = NULL;
    for (size_t i = 0; i < elf->header->program_header_num; i++) {
        elf_program_header_t* program_header = get_elf_program_header(elf, i);
        uint32_t flags_raw = program_header->flags;
        int flags = 0;
        if (flags_raw & 0x1)
            flags |= MMU_BIT_EXECUTE;
        else if (flags_raw & 0x2)
            flags |= MMU_BIT_WRITE;
        if (flags_raw & 0x4)
            flags |= MMU_BIT_READ;

        uint64_t page_count = (program_header->memory_size + PAGE_SIZE - 1) / PAGE_SIZE;
        for (uint64_t i = 0; i < page_count; i++) {
            void* virtual = (void*) program_header->virtual_addr + i * PAGE_SIZE;
            void* page = mmu_alloc(top, virtual, flags | MMU_BIT_USER);

            if ((page_t*) virtual > max_page)
                max_page = virtual;

            if (i * PAGE_SIZE < program_header->file_size) {
                memcpy(page, (void*) elf->header + program_header->offset + i * PAGE_SIZE, (program_header->file_size < (i + 1) * PAGE_SIZE ? program_header->file_size - i * PAGE_SIZE : PAGE_SIZE));
            }
        }
    }

    for (size_t i = 1; i <= stack_size; i++) {
        mmu_alloc(top, max_page + i, MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
    }
    processes[pid].last_virtual_page = (void*) max_page + PAGE_SIZE * (stack_size + 1);
    processes[pid].xs[REGISTER_SP] = (uint64_t) processes[pid].last_virtual_page - 8;
    processes[pid].xs[REGISTER_FP] = processes[pid].xs[REGISTER_SP];
    processes[pid].last_virtual_page += PAGE_SIZE;

    process_t* parent = get_process(parent_pid);
    if (parent != NULL)
        processes[pid].user = parent->user;
    else
        processes[pid].user = 0;
    processes[pid].pid = pid;
    processes[pid].mmu_data = top;
    processes[pid].pc = elf->header->entry;
    processes[pid].state = PROCESS_STATE_WAIT;
    return pid;
}

// switch_to_process(trap_t*, pid_t) -> void
// Jumps to the given process.
void switch_to_process(trap_t* trap, pid_t pid) {
    if (processes[trap->pid].state == PROCESS_STATE_RUNNING) {
        processes[trap->pid].state = PROCESS_STATE_WAIT;
        processes[trap->pid].pc = trap->pc;

        for (int i = 0; i < 32; i++) {
            processes[trap->pid].xs[i] = trap->xs[i];
        }

        for (int i = 0; i < 32; i++) {
            processes[trap->pid].fs[i] = trap->fs[i];
        }
    }

    trap->pid = pid;
    trap->pc = processes[pid].pc;

    for (int i = 0; i < 32; i++) {
        trap->xs[i] = processes[pid].xs[i];
    }

    for (int i = 0; i < 32; i++) {
        trap->fs[i] = processes[pid].fs[i];
    }

    set_mmu(processes[pid].mmu_data);

    processes[pid].state = PROCESS_STATE_RUNNING;
}

// get_next_waiting_process(pid_t) -> pid_t
// Searches for the next waiting process. Returns the given pid if not found.
pid_t get_next_waiting_process(pid_t pid) {
    for (pid_t p = pid + 1; p != pid; p = (p + 1 < MAX_PID ? p + 1 : 0)) {
        if (processes[p].state == PROCESS_STATE_WAIT)
            return p;
    }

    return pid;
}

// get_process(pid_t) -> process_t*
// Gets the process associated with the pid.
process_t* get_process(pid_t pid) {
    if (processes[pid].state == PROCESS_STATE_DEAD)
        return NULL;
    return &processes[pid];
}
