#include <stdbool.h>

#include "console.h"
#include "interrupt.h"
#include "memory.h"
#include "mmu.h"
#include "process.h"
#include "schedulers/scheduler.h"
#include "string.h"

static struct s_task *tasks = NULL;
static pid_t max_pid = 0;

void init_processes(pid_t max) {
    tasks = malloc(max * sizeof(struct s_task));
    max_pid = max;
}

struct s_task *get_task(pid_t pid) {
    if (pid >= max_pid || pid < 0)
        return NULL;
    return &tasks[pid];
}

// spawn_process_from_elf(char*, size_t, elf_t*, size_t, size_t, char**) -> process_t*
// Spawns a process using the given elf file. Returns NULL on failure.
struct s_task *spawn_task_from_elf(char* name, size_t name_size, elf_t* elf, size_t stack_size, size_t argc, char** args) {
    if (elf->header->type != ELF_EXECUTABLE) {
        console_puts("ELF file is not executable\n");
        return NULL;
    }

    pid_t pid;
    for (pid = 0; pid < max_pid && tasks[pid].state != TASK_STATE_DEAD; pid++);
    if (pid == max_pid)
        return NULL;

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
        if (flags_raw & 0x2)
            flags |= MMU_BIT_WRITE;
        else if (flags_raw & 0x1)
            flags |= MMU_BIT_EXEC;
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
    task->ppid = 0;
    task->tid = pid;
    task->gid = pid;
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
    schedule_task(task->pid, task->state, task->priority);
    return task;
}

/*
struct s_task {
    char name[TASK_NAME_SIZE];
    pid_t pid;
    pid_t ppid;
    pid_t gid;
    pid_t tid;

    task_state_t state;
    struct mmu_root mmu_data;

    int priority;
    trap_t trap;
};
*/
struct s_task *spawn_task_from_func(
    pid_t ppid,
    void *func,
    size_t stack_size,
    void* arg
) {
    (void) ppid;
    (void) func;
    (void) stack_size;
    (void) arg;
    return NULL;

    // struct s_task *parent = get_task(ppid);
    // if (!parent)
    //     return NULL;

    // pid_t pid;
    // for (pid = 0; pid < max_pid && tasks[pid].state != TASK_STATE_DEAD; pid++);
    // if (pid == max_pid)
    //     return NULL;

    // struct s_task *task = &tasks[pid];
    // *task = (struct s_task) {
    //     .pid = pid,
    //     .ppid = ppid,
    //     .gid = ppid,
    //     .tid = ppid,

    //     .state = TASK_STATE_READY,
    //     .mmu_data = task->mmu_data,

    //     .priority = task->priority,
    //     .trap = {
    //         .pc = (uint64_t) func,
    //         .interrupt_stack = task->trap.interrupt_stack,
    //         .hartid = task->trap.hartid,
    //         .pid = pid,
    //     },
    // };

    // task->pid = pid;
    // task->trap.pid = pid;
    // task->trap.xs[REGISTER_SP] = (uint64_t) last_virtual_page - 8;
    // task->trap.xs[REGISTER_FP] = task->trap.xs[REGISTER_SP];
}

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
