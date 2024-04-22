#include <stdbool.h>

#include "console.h"
#include "interrupt.h"
#include "memory.h"
#include "mmu.h"
#include "process.h"
#include "string.h"

// hashmap_t* processes;
// atomic_uint_fast64_t processes_lock = 0;

// queue_t* jobs_queue;
// atomic_bool mutating_jobs_queue = false;

// // init_processes() -> void
// // Initialises process related stuff.
// void init_processes() {
//     processes = create_hashmap(sizeof(pid_t), sizeof(process_t));
//     jobs_queue = create_queue(sizeof(pid_t));
// }

static struct s_task *tasks = NULL;
static pid_t max_pid = 0;

void init_processes(pid_t max) {
    tasks = malloc(max * sizeof(struct s_task));
    max_pid = max;
}

// static inline void LOCK_READ_PROCESSES() {
//     while (true) {
//         while (processes_lock & 1);

//         uint64_t old = processes_lock;
//         uint64_t new = old + 2;

//         if (old & 1)
//             continue;

//         bool mutating = false;
//         while (!atomic_compare_exchange_weak(&processes_lock, &old, new)) {
//             if (old & 1) {
//                 mutating = true;
//                 break;
//             }

//             new = old + 2;
//         }

//         if (!mutating)
//             return;
//     }
// }

// static inline void LOCK_WRITE_PROCESSES() {
//     while (true) {
//         if (processes_lock)
//             continue;

//         uint64_t z = 0;
//         if (atomic_compare_exchange_weak(&processes_lock, &z, 1))
//             return;
//     }
// }

// static inline void LOCK_RELEASE_PROCESSES() {
//     // Only one mutable reference allowed, so we dont need an atomic thingy.
//     if (processes_lock == 1)
//         processes_lock = 0;
//     else {
//         uint64_t old = processes_lock;
//         uint64_t new = old - 2;
//         while (!atomic_compare_exchange_weak(&processes_lock, &old, new)) {
//             new = old - 2;
//         }
//     }
// }

struct s_task *get_task(pid_t pid) {
    if (pid >= max_pid || pid < 0)
        return NULL;
    return &tasks[pid];
}

// // get_process(pid_t) -> process_t*
// // Gets the process associated with the pid.
// process_t* get_process(pid_t pid) {
//     uint64_t ra;
//     asm volatile("mv %0, ra" : "=r" (ra));
//     //console_printf("get_process(0x%lx) called from 0x%lx\n", pid, ra);

//     LOCK_READ_PROCESSES();
//     process_t* process = hashmap_get(processes, &pid);
//     if (process == NULL) {
//         LOCK_RELEASE_PROCESSES();
//         return NULL;
//     }

//     bool f = false;
//     while (!atomic_compare_exchange_weak(&process->mutex_lock, &f, true)) {
//         f = false;
//     }

//     return process;
// }

// // get_process_unsafe(pid_t) -> process_t*
// // Gets the process associated with the pid without checking for mutex lock.
// process_t* get_process_unsafe(pid_t pid) {
//     process_t* process = hashmap_get(processes, &pid);
//     if (process == NULL)
//         return NULL;

//     process->mutex_lock = true;
//     return process;
// }

// // process_exists(pid_t) -> bool
// // Checks if the given pid has an associated process.
// bool process_exists(pid_t pid) {
//     process_t* process = get_process(pid);
//     bool exists = process != NULL;
//     unlock_process(process);
//     return exists;
// }

// // unlock_process(process_t*) -> void
// // Unlocks the mutex associated with the process and frees a process hashmap reader.
// void unlock_process(process_t* process) {
//     if (process == NULL)
//         return;
//     process->mutex_lock = false;
//     LOCK_RELEASE_PROCESSES();
// }

// spawn_process_from_elf(char*, size_t, elf_t*, size_t, size_t, char**) -> process_t*
// Spawns a process using the given elf file. Returns NULL on failure.
struct s_task *spawn_task_from_elf(char* name, size_t name_size, elf_t* elf, size_t stack_size, size_t argc, char** args) {
    if (elf->header->type != ELF_EXECUTABLE) {
        console_puts("ELF file is not executable\n");
        return NULL;
    }

    pid_t pid = 0;
    // while (hashmap_get(processes, &pid) != NULL) {
    //     pid++;
    //     if (pid == (pid_t) -1)
    //         pid++;
    // }

    struct mmu_root top;
    if (pid == 0) {
        top = get_mmu();
    } else {
        top = create_mmu_table();
    }

    void* max_page = NULL;
    for (size_t i = 0; i < elf->header->program_header_num; i++) {
        elf_program_header_t* program_header = get_elf_program_header(elf, i);

        if (program_header->type != 1)
            continue;

        uint32_t flags_raw = program_header->flags;
        int flags = 0;
        if (flags_raw & 0x1)
            flags |= MMU_BIT_EXEC;
        else if (flags_raw & 0x2)
            flags |= MMU_BIT_WRITE;
        if (flags_raw & 0x4)
            flags |= MMU_BIT_READ;

        size_t offset = program_header->virtual_addr % PAGE_SIZE;
        uint64_t page_count = (program_header->memory_size + offset + PAGE_SIZE - 1) / PAGE_SIZE;
        for (uint64_t i = 0; i < page_count; i++) {
            void* virt_addr = (void*) program_header->virtual_addr + i * PAGE_SIZE;
            void* page = mmu_alloc(top, virt_addr - offset, flags | MMU_BIT_USER);
            if (!page) {
                struct mmu_entry *entry = mmu_walk_to_entry(top, virt_addr);
                if (!entry
                    || !mmu_entry_valid(*entry)
                    || !mmu_entry_frame(*entry)
                    || !mmu_entry_user(*entry))
                    continue;
                page = mmu_entry_phys(*entry);
            }

            page = phys2safe(page);

            if (virt_addr > max_page)
                max_page = virt_addr;

            size_t size;
            if (program_header->file_size != 0) {
                if (program_header->file_size + offset < i * PAGE_SIZE) {
                    if ((i - 1) * PAGE_SIZE < program_header->file_size + offset)
                        size = program_header->file_size - (i - 1) * PAGE_SIZE;
                    else size = 0;
                } else size = PAGE_SIZE;
                void* source = (void*) elf->header + program_header->offset + i * PAGE_SIZE;
                if (i == 0) {
                    page += offset;
                    if (size > 0)
                        size -= offset;
                } else source -= offset;
                memcpy(page, source, size);
            }
        }
    }

    max_page = (void*) (((intptr_t) max_page + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE);

    for (size_t i = 0; i < stack_size; i++, max_page += PAGE_SIZE) {
        mmu_alloc(top, max_page, MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
    }
    void *last_virtual_page = max_page - PAGE_SIZE;

    struct s_task *task = &tasks[pid];
    task->pid = pid;
    task->trap.pid = pid;
    task->trap.xs[REGISTER_SP] = (uint64_t) last_virtual_page - 8;
    task->trap.xs[REGISTER_FP] = task->trap.xs[REGISTER_SP];
    last_virtual_page += PAGE_SIZE;

    int offset = 0;
    char** args_new = malloc(sizeof(char*) * argc);
    char* physical = mmu_alloc(top, last_virtual_page,
        MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
    physical = phys2safe(physical);
    void* virt_addr = last_virtual_page;
    last_virtual_page += PAGE_SIZE;
    for (size_t arg_index = 0; arg_index < argc; arg_index++) {
        args_new[arg_index] = virt_addr + offset;
        for (size_t i = 0; args[arg_index][i]; i++, offset++) {
            if (offset >= PAGE_SIZE) {
                offset = 0;
                physical = mmu_alloc(top, last_virtual_page,
                    MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
                physical = phys2safe(physical);
                virt_addr = last_virtual_page;
                last_virtual_page += PAGE_SIZE;
            }
            physical[offset] = args[arg_index][i];
        }
        if (offset >= PAGE_SIZE) {
            offset = 0;
            physical = mmu_alloc(top, last_virtual_page,
                MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
            physical = phys2safe(physical);
            virt_addr = last_virtual_page;
            last_virtual_page += PAGE_SIZE;
        }
        physical[offset++] = '\0';
    }

    offset += (-offset) & (sizeof(void*) - 1);
    void* first = virt_addr + offset;

    for (size_t arg_index = 0; arg_index < argc; arg_index++, offset += sizeof(void*)) {
        if (offset >= PAGE_SIZE) {
            offset = 0;
            physical = mmu_alloc(top, last_virtual_page,
                MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
            physical = phys2safe(physical);
            virt_addr = last_virtual_page;
            last_virtual_page += PAGE_SIZE;
        }
        *((void**) (physical + offset)) = args_new[arg_index];
    }

    task->trap.xs[REGISTER_A0] = argc;
    task->trap.xs[REGISTER_A1] = (uint64_t) first;

    if (name_size > TASK_NAME_SIZE - 1)
        name_size = TASK_NAME_SIZE - 1;

    memcpy(task->name, name, name_size);
    task->name[name_size] = '\0';
    task->pid = pid;
    task->mmu_data = top;
    task->trap.pc = elf->header->entry;
    task->state = TASK_STATE_READY;
    return task;
}

// // spawn_process_from_elf(char*, size_t, elf_t*, size_t, size_t, char**) -> process_t*
// // Spawns a process using the given elf file. Returns NULL on failure.
// process_t* spawn_process_from_elf(char* name, size_t name_size, elf_t* elf, size_t stack_size, size_t argc, char** args) {
//     if (elf->header->type != ELF_EXECUTABLE) {
//         console_puts("ELF file is not executable\n");
//         return NULL;
//     }

//     LOCK_WRITE_PROCESSES();
//     pid_t pid = 0;
//     while (hashmap_get(processes, &pid) != NULL) {
//         pid++;
//         if (pid == (pid_t) -1)
//             pid++;
//     }

//     struct mmu_root top;
//     if (pid == 0) {
//         top = get_mmu();
//     } else {
//         top = create_mmu_table();
//     }

//     void* max_page = NULL;
//     for (size_t i = 0; i < elf->header->program_header_num; i++) {
//         elf_program_header_t* program_header = get_elf_program_header(elf, i);

//         if (program_header->type != 1)
//             continue;

//         uint32_t flags_raw = program_header->flags;
//         int flags = 0;
//         if (flags_raw & 0x1)
//             flags |= MMU_BIT_EXEC;
//         else if (flags_raw & 0x2)
//             flags |= MMU_BIT_WRITE;
//         if (flags_raw & 0x4)
//             flags |= MMU_BIT_READ;

//         size_t offset = program_header->virtual_addr % PAGE_SIZE;
//         uint64_t page_count = (program_header->memory_size + offset + PAGE_SIZE - 1) / PAGE_SIZE;
//         for (uint64_t i = 0; i < page_count; i++) {
//             void* virt_addr = (void*) program_header->virtual_addr + i * PAGE_SIZE;
//             void* page = mmu_alloc(top, virt_addr - offset, flags | MMU_BIT_USER);
//             if (page == NULL) {
//                 struct mmu_entry *entry = mmu_walk_to_entry(top, virt_addr);
//                 if (!entry
//                     || !mmu_entry_valid(*entry)
//                     || !mmu_entry_frame(*entry)
//                     || !mmu_entry_user(*entry))
//                     continue;
//                 page = mmu_entry_phys(*entry);
//             }

//             page = phys2safe(page);

//             if (virt_addr > max_page)
//                 max_page = virt_addr;

//             size_t size;
//             if (program_header->file_size != 0) {
//                 if (program_header->file_size + offset < i * PAGE_SIZE) {
//                     if ((i - 1) * PAGE_SIZE < program_header->file_size + offset)
//                         size = program_header->file_size - (i - 1) * PAGE_SIZE;
//                     else size = 0;
//                 } else size = PAGE_SIZE;
//                 void* source = (void*) elf->header + program_header->offset + i * PAGE_SIZE;
//                 if (i == 0) {
//                     page += offset;
//                     if (size > 0)
//                         size -= offset;
//                 } else source -= offset;
//                 memcpy(page, source, size);
//             }
//         }
//     }

//     max_page = (void*) (((intptr_t) max_page + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE);

//     mmu_alloc(top, max_page, MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
//     void* fault_stack = max_page + PAGE_SIZE;
//     for (size_t i = 1; i <= stack_size; i++) {
//         mmu_alloc(top, max_page + i * PAGE_SIZE, MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
//     }
//     process_t process = { 0 };
//     process.fault_stack = fault_stack;
//     process.last_virtual_page = (void*) max_page + PAGE_SIZE * (stack_size + 1);
//     process.fault_stack = fault_stack;
//     process.xs[REGISTER_SP] = (uint64_t) process.last_virtual_page - 8;
//     process.xs[REGISTER_FP] = process.xs[REGISTER_SP];
//     process.last_virtual_page += PAGE_SIZE;

//     int offset = 0;
//     char** args_new = malloc(sizeof(char*) * argc);
//     char* physical = mmu_alloc(top, process.last_virtual_page,
//         MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
//     physical = phys2safe(physical);
//     void* virt_addr = process.last_virtual_page;
//     process.last_virtual_page += PAGE_SIZE;
//     for (size_t arg_index = 0; arg_index < argc; arg_index++) {
//         args_new[arg_index] = virt_addr + offset;
//         for (size_t i = 0; args[arg_index][i]; i++, offset++) {
//             if (offset >= PAGE_SIZE) {
//                 offset = 0;
//                 physical = mmu_alloc(top, process.last_virtual_page,
//                     MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
//                 physical = phys2safe(physical);
//                 virt_addr = process.last_virtual_page;
//                 process.last_virtual_page += PAGE_SIZE;
//             }
//             physical[offset] = args[arg_index][i];
//         }
//         if (offset >= PAGE_SIZE) {
//             offset = 0;
//             physical = mmu_alloc(top, process.last_virtual_page,
//                 MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
//             physical = phys2safe(physical);
//             virt_addr = process.last_virtual_page;
//             process.last_virtual_page += PAGE_SIZE;
//         }
//         physical[offset++] = '\0';
//     }

//     offset += (-offset) & (sizeof(void*) - 1);
//     void* first = virt_addr + offset;

//     for (size_t arg_index = 0; arg_index < argc; arg_index++, offset += sizeof(void*)) {
//         if (offset >= PAGE_SIZE) {
//             offset = 0;
//             physical = mmu_alloc(top, process.last_virtual_page,
//                 MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
//             physical = phys2safe(physical);
//             virt_addr = process.last_virtual_page;
//             process.last_virtual_page += PAGE_SIZE;
//         }
//         *((void**) (physical + offset)) = args_new[arg_index];
//     }

//     process.xs[REGISTER_A0] = argc;
//     process.xs[REGISTER_A1] = (uint64_t) first;

//     if (name_size > PROCESS_NAME_SIZE)
//         name_size = PROCESS_NAME_SIZE;

//     process.name = malloc(name_size + 1);
//     memcpy(process.name, name, name_size);
//     process.name[name_size] = '\0';
//     process.pid = pid;
//     process.thread_source = -1;
//     process.mmu_data = top;
//     process.pc = elf->header->entry;
//     process.state = PROCESS_STATE_READY;
//     process.capabilities = NULL;
//     process.capabilities_cap = 0;
//     process.capabilities_len = 0;
//     process.fault_handler = NULL;
//     process.faulted = false;
//     hashmap_insert(processes, &pid, &process);
//     process_t* process_ptr = hashmap_get(processes, &pid);
//     process_ptr->mutex_lock = true;
//     processes_lock = processes_lock << 1;
//     bool f = false;
//     while (!atomic_compare_exchange_weak(&mutating_jobs_queue, &f, true)) {
//         f = false;
//     }

//     queue_enqueue(jobs_queue, &pid);
//     mutating_jobs_queue = false;
//     return process_ptr;
// }

// // spawn_thread_from_func(pid_t, void*, size_t, void*) -> process_t*
// // Spawns a thread from the given process. Returns NULL on failure.
// process_t* spawn_thread_from_func(pid_t parent_pid, void* func, size_t stack_size, void* args) {
//     LOCK_WRITE_PROCESSES();
//     process_t* parent = hashmap_get(processes, &parent_pid);
//     if (parent != NULL) {
//         bool f = false;
//         while (!atomic_compare_exchange_weak(&parent->mutex_lock, &f, true)) {
//             f = false;
//         }
//     } else {
//         LOCK_RELEASE_PROCESSES();
//         return NULL;
//     }
//     if (parent->thread_source != (uint64_t) -1) {
//         pid_t source = parent->thread_source;
//         parent->mutex_lock = false;
//         parent = hashmap_get(processes, &source);
//         if (parent != NULL) {
//             bool f = false;
//             while (!atomic_compare_exchange_weak(&parent->mutex_lock, &f, true)) {
//                 f = false;
//             }
//         } else {
//             LOCK_RELEASE_PROCESSES();
//             return NULL;
//         }
//     }

//     pid_t pid = 0;
//     while (hashmap_get(processes, &pid) != NULL) {
//         pid++;
//         if (pid == (pid_t) -1)
//             pid++;
//     }

//     process_t process = { 0 };
//     process.mmu_data = parent->mmu_data;
//     process.thread_source = parent->pid;

//     parent->last_virtual_page += PAGE_SIZE;
//     mmu_alloc(parent->mmu_data, parent->last_virtual_page, MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
//     parent->last_virtual_page += PAGE_SIZE;
//     void* fault_stack = parent->last_virtual_page;
//     for (size_t i = 1; i <= stack_size; i++) {
//         mmu_alloc(parent->mmu_data, parent->last_virtual_page + PAGE_SIZE * i, MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
//     }
//     parent->last_virtual_page += PAGE_SIZE * (stack_size + 1);
//     process.xs[REGISTER_SP] = (uint64_t) parent->last_virtual_page - 8;
//     process.xs[REGISTER_FP] = process.xs[REGISTER_SP];
//     process.fault_stack = fault_stack;
//     parent->last_virtual_page += PAGE_SIZE;

//     process.xs[REGISTER_A0] = (uint64_t) args;

//     size_t name_size = 0;
//     for (; parent->name[name_size]; name_size++);
//     if (name_size < PROCESS_NAME_SIZE && parent->name[name_size - 1] != '~')
//         name_size++;
//     process.name = malloc(name_size + 1);
//     memcpy(process.name, parent->name, name_size - 1);
//     process.name[name_size - 1] = '~';
//     process.name[name_size] = '\0';
//     process.pid = pid;
//     process.pc = (uint64_t) func;
//     process.state = PROCESS_STATE_READY;
//     process.capabilities = NULL;
//     process.capabilities_cap = 0;
//     process.capabilities_len = 0;
//     process.fault_handler = NULL;
//     process.faulted = false;
//     hashmap_insert(processes, &pid, &process);
//     parent->mutex_lock = false;
//     process_t* process_ptr = hashmap_get(processes, &pid);
//     process_ptr->mutex_lock = true;
//     processes_lock = processes_lock << 1;

//     bool f = false;
//     while (!atomic_compare_exchange_weak(&mutating_jobs_queue, &f, true)) {
//         f = false;
//     }

//     queue_enqueue(jobs_queue, &pid);
//     mutating_jobs_queue = false;

//     return process_ptr;
// }

// // save_process(trap_t*) -> void
// // Saves a process and pushes it onto the queue.
// void save_process(trap_t* trap) {
//     process_t* last = get_process(trap->pid);
//     if (last == NULL) {
//         unlock_process(last);
//         return;
//     }

//     if (last->state == PROCESS_STATE_RUNNING)
//         last->state = PROCESS_STATE_READY;
//     last->pc = trap->pc;

//     for (int i = 0; i < 32; i++) {
//         last->xs[i] = trap->xs[i];
//     }

//     for (int i = 0; i < 32; i++) {
//         last->fs[i] = trap->fs[i];
//     }

//     bool f = false;
//     while (!atomic_compare_exchange_weak(&mutating_jobs_queue, &f, true)) {
//         f = false;
//     }

//     queue_enqueue(jobs_queue, &trap->pid);
//     mutating_jobs_queue = false;

//     unlock_process(last);
// }

// // switch_to_process(trap_t*, pid_t) -> void
// // Jumps to the given process.
// void switch_to_process(trap_t* trap, pid_t pid) {
//     process_t* last = trap->pid != pid ? get_process(trap->pid) : NULL;
//     process_t* next = get_process(pid);

//     if (last != NULL) {
//         bool f = false;
//         while (!atomic_compare_exchange_weak(&mutating_jobs_queue, &f, true)) {
//             f = false;
//         }

//         queue_enqueue(jobs_queue, &trap->pid);
//         mutating_jobs_queue = false;

//         if (last->state == PROCESS_STATE_RUNNING)
//             last->state = PROCESS_STATE_READY;
//         last->pc = trap->pc;

//         for (int i = 0; i < 32; i++) {
//             last->xs[i] = trap->xs[i];
//         }

//         for (int i = 0; i < 32; i++) {
//             last->fs[i] = trap->fs[i];
//         }
//     }

//     if (trap->pid != pid) {
//         trap->pid = next->pid;
//         trap->pc = next->pc;

//         for (int i = 0; i < 32; i++) {
//             trap->xs[i] = next->xs[i];
//         }

//         for (int i = 0; i < 32; i++) {
//             trap->fs[i] = next->fs[i];
//         }

//         next->state = PROCESS_STATE_RUNNING;
//     }

//     set_mmu(next->mmu_data);

//     if (last != next)
//         unlock_process(last);
//     unlock_process(next);
// }

// // get_next_waiting_process(pid_t) -> pid_t
// // Searches for the next waiting process. Returns -1 if not found.
// pid_t get_next_waiting_process(pid_t pid) {
//     bool f = false;
//     while (!atomic_compare_exchange_weak(&mutating_jobs_queue, &f, true)) {
//         f = false;
//     }

//     size_t len = queue_len(jobs_queue);
//     while (len) {
//         pid_t next_pid;
//         queue_dequeue(jobs_queue, &next_pid);

//         if (next_pid == pid) {
//             len--;
//             continue;
//         }

//         process_t* process = get_process(next_pid);
//         if (process == NULL) {
//             len--;
//             continue;
//         }

//         if (process->state == PROCESS_STATE_READY) {
//             mutating_jobs_queue = false;
//             unlock_process(process);
//             return next_pid;
//         } else if (process->state == PROCESS_STATE_BLOCK_SLEEP) {
//             time_t wait = process->wake_on_time;
//             time_t now = get_time();

//             if ((wait.seconds == now.seconds && wait.micros <= now.micros) || (wait.seconds < now.seconds)) {
//                 mutating_jobs_queue = false;
//                 process->state = PROCESS_STATE_READY;
//                 unlock_process(process);
//                 return next_pid;
//             }
//         } else if (process->state == PROCESS_STATE_BLOCK_LOCK) {
//             struct mmu_root current = get_mmu();
//             set_mmu(process->mmu_data);
//             if (lock_stop(process->lock_ref, process->lock_type, process->lock_value)) {
//                 process->xs[REGISTER_A0] = 0;
//                 mutating_jobs_queue = false;
//                 unlock_process(process);
//                 return next_pid;
//             }
//             set_mmu(current);
//         }

//         unlock_process(process);
//         queue_enqueue(jobs_queue, &next_pid);
//         len--;
//     }

//     mutating_jobs_queue = false;
//     process_t* current = get_process(pid);
//     if (current && current->state == PROCESS_STATE_RUNNING) {
//         unlock_process(current);
//         return pid;
//     }

//     unlock_process(current);
//     return (pid_t) -1;
// }

// // push_capability(pid_t, capability_internal_t) -> void
// // Pushes a capability to a process's list of capabilities.
// void push_capability(pid_t pid, capability_internal_t cap) {
//     process_t* process = get_process(pid);
//     if (process->capabilities_len >= process->capabilities_cap) {
//         if (process->capabilities_cap == 0) {
//             process->capabilities_cap = 8;
//         } else process->capabilities_cap <<= 1;
//         process->capabilities = realloc(process->capabilities, sizeof(capability_internal_t) * process->capabilities_cap);
//     }
//     process->capabilities[process->capabilities_len++] = cap;
//     unlock_process(process);
// }

// // kill_process(pid_t) -> void
// // Kills a process.
// void kill_process(pid_t pid) {
//     LOCK_WRITE_PROCESSES();
//     process_t* process = hashmap_get(processes, &pid);
//     if (process == NULL) {
//         LOCK_RELEASE_PROCESSES();
//         return;
//     }

//     bool f = false;
//     while (!atomic_compare_exchange_weak(&process->mutex_lock, &f, true)) {
//         f = false;
//     }

//     pid_t initd = 0;
//     if (mmu_root_equal(process->mmu_data, get_mmu()))
//         set_mmu(((process_t*) hashmap_get(processes, &initd))->mmu_data); // initd's mmu pointer never changes so this is okay

//     if (process->thread_source == (pid_t) -1)
//         clean_mmu_table(process->mmu_data);
//     process->mutex_lock = false;
//     hashmap_remove(processes, &pid);
//     LOCK_RELEASE_PROCESSES();

//     // Update harts
//     sbi_send_ipi(0, -1);
// }

