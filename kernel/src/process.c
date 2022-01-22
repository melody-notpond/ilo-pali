#include <stdbool.h>

#include "console.h"
#include "hashmap.h"
#include "memory.h"
#include "process.h"

process_t* processes;
hashmap_t* capabilities;

pid_t MAX_PID = 1024;
pid_t current_pid = 0;

// init_processes() -> void
// Initialises process related stuff.
void init_processes() {
    processes = malloc(MAX_PID * sizeof(process_t));
    capabilities = create_hashmap(sizeof(capability_t), sizeof(process_channel_t));
}

// spawn_process_from_elf(pid_t, elf_t*, size_t, void*, size_t) -> pid_t
// Spawns a process using the given elf file and parent pid. Returns -1 on failure.
pid_t spawn_process_from_elf(pid_t parent_pid, elf_t* elf, size_t stack_size, void* args, size_t arg_size) {
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
            void* page = kernel_space_phys2virtual(mmu_alloc(top, virtual, flags | MMU_BIT_USER));

            if ((page_t*) virtual > max_page)
                max_page = virtual;

            if (i * PAGE_SIZE < program_header->file_size) {
                memcpy(page, (void*) elf->header + program_header->offset + i * PAGE_SIZE, (program_header->file_size < (i + 1) * PAGE_SIZE ? program_header->file_size - i * PAGE_SIZE : PAGE_SIZE));
            }
        }
    }

    max_page = (void*) (((intptr_t) max_page + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE);

    for (size_t i = 1; i <= stack_size; i++) {
        mmu_alloc(top, max_page + i, MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
    }
    processes[pid].last_virtual_page = (void*) max_page + PAGE_SIZE * (stack_size + 1);
    processes[pid].xs[REGISTER_SP] = (uint64_t) processes[pid].last_virtual_page - 8;
    processes[pid].xs[REGISTER_FP] = processes[pid].xs[REGISTER_SP];
    processes[pid].last_virtual_page += PAGE_SIZE;

    if (args != NULL && arg_size != 0) {
        for (size_t i = 0; i < (arg_size + PAGE_SIZE - 1); i += PAGE_SIZE) {
            void* physical = mmu_alloc(top, processes[pid].last_virtual_page + i, MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
            memcpy(kernel_space_phys2virtual(physical), args + i, arg_size - i < PAGE_SIZE ? arg_size - i : PAGE_SIZE);
        }

        processes[pid].xs[REGISTER_A0] = (uint64_t) processes[pid].last_virtual_page;
        processes[pid].xs[REGISTER_A1] = arg_size;
        processes[pid].last_virtual_page += (arg_size + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE + PAGE_SIZE;
    }

    process_t* parent = get_process(parent_pid);
    if (parent != NULL)
        processes[pid].user = parent->user;
    else
        processes[pid].user = 0;
    processes[pid].pid = pid;
    processes[pid].thread_source = -1;
    processes[pid].mmu_data = top;
    processes[pid].pc = elf->header->entry;
    processes[pid].state = PROCESS_STATE_WAIT;
    return pid;
}

// spawn_thread_from_func(pid_t, void*, size_t, void*, size_t) -> pid_t
// Spawns a thread from the given process. Returns -1 on failure.
pid_t spawn_thread_from_func(pid_t parent_pid, void* func, size_t stack_size, void* args, size_t arg_size) {
    process_t* parent = get_process(parent_pid);
    if (parent == NULL)
        return -1;
    if (parent->thread_source != (uint64_t) -1)
        parent = get_process(parent->thread_source);
    if (parent == NULL)
        return -1;

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

    processes[pid].mmu_data = parent->mmu_data;
    processes[pid].thread_source = parent->pid;
    processes[pid].user = parent->user;

    for (size_t i = 1; i <= stack_size; i++) {
        mmu_alloc(parent->mmu_data, parent->last_virtual_page + PAGE_SIZE * i, MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
    }
    parent->last_virtual_page += PAGE_SIZE * (stack_size + 1);
    processes[pid].xs[REGISTER_SP] = (uint64_t) parent->last_virtual_page - 8;
    processes[pid].xs[REGISTER_FP] = processes[pid].xs[REGISTER_SP];
    parent->last_virtual_page += PAGE_SIZE;

    processes[pid].xs[REGISTER_A0] = (uint64_t) args;
    processes[pid].xs[REGISTER_A1] = arg_size;

    processes[pid].pid = pid;
    processes[pid].pc = (uint64_t) func;
    processes[pid].state = PROCESS_STATE_WAIT;
    return pid;
}

// create_capability(capability_t*, capability_t*) -> void
// Creates a capability pair. The provided pointers are set to the capabilities.
void create_capability(capability_t* a, capability_t* b) {
    *a = 0;
    *b = 0;
    do {
        // TODO: cryptographically secure random function
        *a += 1;
    } while (hashmap_get(capabilities, a) != NULL);

    hashmap_insert(capabilities, a, &(process_channel_t) {
        .message_queue = malloc(sizeof(process_message_t) * PROCESS_MESSAGE_QUEUE_SIZE),
        .start = 0,
        .end = 0,
        .len = 0,
        .receiver = 0,
    });

    do {
        // TODO: cryptographically secure random function
        *b += 1;
    } while (hashmap_get(capabilities, b) != NULL);

    hashmap_insert(capabilities, b, &(process_channel_t) {
        .message_queue = malloc(sizeof(process_message_t) * PROCESS_MESSAGE_QUEUE_SIZE),
        .start = 0,
        .end = 0,
        .len = 0,
        .sender = *a,
        .receiver = 0,
    });

    ((process_channel_t*) hashmap_get(capabilities, a))->sender = *b;
}

// switch_to_process(trap_t*, pid_t) -> void
// Jumps to the given process.
void switch_to_process(trap_t* trap, pid_t pid) {
    if (processes[trap->pid].state != PROCESS_STATE_DEAD && processes[trap->pid].state != PROCESS_STATE_WAIT) {
        if (processes[trap->pid].state == PROCESS_STATE_RUNNING)
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
    while (true) {
        for (pid_t p = pid + 1; p != pid; p = (p + 1 < MAX_PID ? p + 1 : 0)) {
            if (processes[p].thread_source != (uint64_t) -1 && processes[processes[p].thread_source].state == PROCESS_STATE_DEAD) {
                kill_process(p);
                continue;
            }

            if (processes[p].state == PROCESS_STATE_WAIT)
                return p;
            else if (processes[p].state == PROCESS_STATE_BLOCK_SLEEP) {
                time_t wait = processes[p].wake_on_time;
                time_t now = get_time();
                if ((wait.seconds == now.seconds && wait.micros <= now.micros) || (wait.seconds < now.seconds)) {
                    processes[p].xs[REGISTER_A0] = now.seconds;
                    processes[p].xs[REGISTER_A1] = now.micros;
                    return p;
                }
            } else if (processes[p].state == PROCESS_STATE_BLOCK_LOCK) {
                mmu_level_1_t* current = get_mmu();
                if (current != processes[p].mmu_data)
                    set_mmu(processes[p].mmu_data);
                if (lock_stop(processes[p].lock_ref, processes[p].lock_type, processes[p].lock_value)) {
                    processes[p].xs[REGISTER_A0] = 0;
                    return p;
                }
                if (current != processes[p].mmu_data)
                    set_mmu(current);
            }
        }

        if (processes[pid].state == PROCESS_STATE_RUNNING)
            return pid;
        else if (processes[pid].state == PROCESS_STATE_BLOCK_SLEEP) {
            time_t wait = processes[pid].wake_on_time;
            time_t now = get_time();
            if ((wait.seconds == now.seconds && wait.micros <= now.micros) || (wait.seconds < now.seconds)) {
                processes[pid].xs[REGISTER_A0] = now.seconds;
                processes[pid].xs[REGISTER_A1] = now.micros;
                return pid;
            }
        } else if (processes[pid].state == PROCESS_STATE_BLOCK_LOCK && lock_stop(processes[pid].lock_ref, processes[pid].lock_type, processes[pid].lock_value)) {
            processes[pid].xs[REGISTER_A0] = 0;
            return pid;
        }
    }
}

// get_process(pid_t) -> process_t*
// Gets the process associated with the pid.
process_t* get_process(pid_t pid) {
    if (processes[pid].state == PROCESS_STATE_DEAD)
        return NULL;
    return &processes[pid];
}

static bool find_associated_dead_capability(void* data, void* _key, void* value) {
    process_channel_t* channel = value;
    pid_t pid = (pid_t) data;
    return channel->receiver == pid;
}

// kill_process(pid_t) -> void
// Kills a process.
void kill_process(pid_t pid) {
    if (processes[pid].state == PROCESS_STATE_DEAD)
        return;

    size_t i = 0, j = 0;
    process_channel_t* channel;
    while ((channel = hashmap_find(capabilities, (void*) pid, find_associated_dead_capability, &i, &j))) {
        free(channel->message_queue);
        channel->message_queue = NULL;
        channel->start = 0;
        channel->end = 0;
        channel->len = 0;
        channel->receiver = 0;
    }

    if (processes[pid].mmu_data == get_mmu())
        set_mmu(processes[0].mmu_data);

    if (processes[pid].thread_source == (pid_t) -1)
        clean_mmu_table(processes[pid].mmu_data);
    processes[pid].state = PROCESS_STATE_DEAD;
}

// enqueue_message_to_channel(capability_t, process_message_t) -> int
// Enqueues a message to a channel's message queue. Returns 0 if successful, 1 if the capability is invalid, 2 if the queue is full, and 3 if the connection has closed.
int enqueue_message_to_channel(capability_t capability, process_message_t message) {
    process_channel_t* sender_channel = hashmap_get(capabilities, &capability);
    if (sender_channel == NULL)
        return 1;
    process_channel_t* channel = hashmap_get(capabilities, &sender_channel->sender);
    if (channel == NULL)
        return 1;

    if (channel->message_queue == NULL)
        return 3;

    if (channel->len >= PROCESS_MESSAGE_QUEUE_SIZE)
        return 2;

    channel->message_queue[channel->end++] = message;
    if (channel->end >= PROCESS_MESSAGE_QUEUE_SIZE)
        channel->end = 0;
    channel->len++;
    return 0;
}

// enqueue_interrupt_to_channel(capability_t, uint32_t) -> int
// Enqueues an interrupt to a channel's message queue. Returns 0 if successful, 1 if the capability is invalid, 2 if the queue is full, and 3 if the connection has closed.
int enqueue_interrupt_to_channel(capability_t capability, uint32_t id) {
    process_channel_t* channel = hashmap_get(capabilities, &capability);
    if (channel == NULL)
        return 1;

    if (channel->message_queue == NULL)
        return 3;

    if (channel->len >= PROCESS_MESSAGE_QUEUE_SIZE)
        return 2;

    channel->message_queue[channel->end++] = (process_message_t) {
        .type = 4,
        .source = 0,
        .data = id,
        .metadata = 0,
    };
    if (channel->end >= PROCESS_MESSAGE_QUEUE_SIZE)
        channel->end = 0;
    channel->len++;
    return 0;
}

// dequeue_message_from_channel(pid_t, capability_t, process_message_t*) -> int
// Dequeues a message from a channel's message queue. Returns 0 if successful, 1 if the capability is invalid, 2 if the queue is empty, and 3 if the channel has closed.
int dequeue_message_from_channel(pid_t pid, capability_t capability, process_message_t* message) {
    process_channel_t* channel = hashmap_get(capabilities, &capability);
    if (channel == NULL)
        return 1;

    if (channel->message_queue == NULL)
        return 3;

    channel->receiver = pid;

    if (channel->len == 0)
        return 2;

    *message = channel->message_queue[channel->start++];
    if (channel->start >= PROCESS_MESSAGE_QUEUE_SIZE)
        channel->start = 0;
    channel->len--;
    return 0;
}
