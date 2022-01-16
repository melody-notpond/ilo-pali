#include <stddef.h>

#include "console.h"
#include "interrupt.h"
#include "memory.h"
#include "process.h"
#include "opensbi.h"
#include "time.h"

void timer_switch(trap_t* trap) {
    pid_t next_pid = get_next_waiting_process(trap->pid);
    switch_to_process(trap, next_pid);
    time_t next = get_time();
    next.micros += 10;
    set_next_time_interrupt(next);
}

trap_t* interrupt_handler(uint64_t cause, trap_t* trap) {
    //console_printf("cause: %lx\ntrap location: %lx\n", cause, trap->pc);

    if (cause & 0x8000000000000000) {
        cause &= 0x7fffffffffffffff;

        switch (cause) {
            // Software interrupt
            case 1:
                while(1);

            // Timer interrupt
            case 5:
                timer_switch(trap);
                break;

            // External interrupt
            case 9:
                while(1);
                break;

            // Everything else is either invalid or handled by the firmware.
            default:
                while(1);
        }
    } else {
        switch (cause) {
            // Instruction address misaligned
            case 0:

            // Instruction access fault
            case 1:

            // Illegal instruction
            case 2:

            // Breakpoint
            case 3:

            // Load address misaligned
            case 4:

            // Load access fault
            case 5:

            // Store address misaligned
            case 6:

            // Store access fault
            case 7:
                while(1);

            // Environment call (ie, syscall)
            case 8:
                trap->pc += 4;
                switch (trap->xs[REGISTER_A0]) {
                    // uart_write(char* data, size_t length) -> void
                    case 0: {
                        char* data = (void*) trap->xs[REGISTER_A1];
                        size_t length = trap->xs[REGISTER_A2];

                        for (size_t i = 0; i < length; i++) {
                            sbi_console_putchar(data[i]);
                        }

                        trap->xs[REGISTER_A0] = 0;
                        break;
                    }

                    // alloc_page(void* addr, size_t count, int permissions) -> void* addr
                    //
                    // Allocates `count` pages of memory containing addr. If addr is NULL, then it allocates the next available page. Returns NULL on failure. Write and execute cannot both be set at the same time.
                    case 1: {
                        void* addr = (void*) trap->xs[REGISTER_A1];
                        size_t count = trap->xs[REGISTER_A2];
                        int perms = trap->xs[REGISTER_A3];
                        int flags = 0;

                        if ((perms & 0x03) == 0x03 || (perms & 0x07) == 0 || count == 0) {
                            trap->xs[REGISTER_A0] = 0;
                            break;
                        }

                        if (perms & 0x01)
                            flags |= MMU_BIT_EXECUTE;
                        else if (perms & 0x02)
                            flags |= MMU_BIT_WRITE;
                        if (perms & 0x04)
                            flags |= MMU_BIT_READ;

                        if (addr == NULL) {
                            process_t* process = get_process(trap->pid);
                            addr = process->last_virtual_page;

                            for (size_t i = 0; i < count; i++) {
                                void* page = mmu_alloc(process->mmu_data, process->last_virtual_page, flags | MMU_BIT_USER);

                                if (!page) {
                                    process->last_virtual_page -= PAGE_SIZE * i;
                                    trap->xs[REGISTER_A0] = 0;

                                    for (size_t j = 0; j < i; j++) {
                                        mmu_remove(process->mmu_data, addr + j * PAGE_SIZE);
                                    }

                                    return trap;
                                }

                                process->last_virtual_page += PAGE_SIZE;
                            }
                            trap->xs[REGISTER_A0] = (uint64_t) addr;
                        } else if (trap->pid == 0) {
                            if (addr < get_memory_start()) {
                                process_t* process = get_process(trap->pid);
                                mmu_map(process->mmu_data, process->last_virtual_page, addr, flags | MMU_BIT_USER);
                                trap->xs[REGISTER_A0] = (uint64_t) process->last_virtual_page;
                                process->last_virtual_page += PAGE_SIZE;
                            } else trap->xs[REGISTER_A0] = 0;
                        } else trap->xs[REGISTER_A0] = 0;

                        break;
                    }

                    // page_permissions(void* addr, size_t count, int permissions) -> int status
                    // Modifies the permissions of the given pages. Returns 0 on success, 1 if the page was never allocated, and 2 if invalid permissions.
                    //
                    // Permissions:
                    // - READ    - 0b100
                    // - WRITE   - 0b010
                    // - EXECUTE - 0b001
                    case 2: {
                        void* addr = (void*) trap->xs[REGISTER_A1];
                        size_t count = trap->xs[REGISTER_A2];
                        int perms = trap->xs[REGISTER_A3];
                        int flags = 0;

                        if ((perms & 0x03) == 0x03 || (perms & 0x07) == 0 || count == 0) {
                            trap->xs[REGISTER_A0] = 2;
                            break;
                        }

                        if (perms & 0x01)
                            flags |= MMU_BIT_EXECUTE;
                        else if (perms & 0x02)
                            flags |= MMU_BIT_WRITE;
                        if (perms & 0x04)
                            flags |= MMU_BIT_READ;

                        process_t* process = get_process(trap->pid);

                        for (size_t i = 0; i < count; i++) {
                            intptr_t page = mmu_walk(process->mmu_data, addr + i * PAGE_SIZE);
                            if (page & MMU_BIT_USER) {
                                mmu_change_flags(process->mmu_data, addr + i * PAGE_SIZE, flags | MMU_BIT_USER);
                            } else {
                                trap->xs[REGISTER_A0] = 1;
                                return trap;
                            }
                        }

                        trap->xs[REGISTER_A0] = 0;
                        clean_mmu_table(process->mmu_data);
                        flush_mmu();
                        break;
                    }

                    // dealloc_page(void* addr, size_t count) -> int status
                    // Deallocates the page(s) containing the given address. Returns 0 on success and 1 if a page was never allocated by this process.
                    case 3: {
                        void* addr = (void*) trap->xs[REGISTER_A0];
                        size_t count = trap->xs[REGISTER_A1];

                        process_t* process = get_process(trap->pid);
                        for (size_t i = 0; i < count; i++) {
                            if (mmu_walk(process->mmu_data, addr + i * PAGE_SIZE) & MMU_BIT_USER) {
                                void* physical = mmu_remove(process->mmu_data, addr + i * PAGE_SIZE);
                                dealloc_pages(physical, 1);
                            } else {
                                trap->xs[REGISTER_A0] = 1;
                                return trap;
                            }
                        }

                        trap->xs[REGISTER_A0] = 0;
                        break;
                    }

                    // getpid() -> pid_t
                    // Gets the pid of the current process.
                    case 4:
                        trap->xs[REGISTER_A0] = trap->pid;
                        break;

                    // getuid(pid_t pid) -> uid_t
                    // Gets the uid of the given process. Returns -1 if the process doesn't exist.
                    case 5: {
                        pid_t pid = trap->xs[REGISTER_A1];
                        process_t* process = get_process(pid);
                        if (process)
                            trap->xs[REGISTER_A0] = process->user;
                        else
                            trap->xs[REGISTER_A0] = -1;
                        break;
                    }

                    // setuid(pid_t pid, uid_t uid) -> int status
                    // Sets the uid of the given process (can only be done by processes with uid = 0). Returns 0 on success, 1 if the process does not exist, and 2 if insufficient permissions.
                    case 6: {
                        pid_t pid = trap->xs[REGISTER_A1];
                        uid_t uid = trap->xs[REGISTER_A2];

                        process_t* current = get_process(trap->pid);

                        if (current->user != 0) {
                            trap->xs[REGISTER_A0] = 2;
                            break;
                        }

                        process_t* process = get_process(pid);
                        if (process) {
                            process->user = uid;
                            trap->xs[REGISTER_A0] = 0;
                        } else trap->xs[REGISTER_A0] = 1;
                        break;
                    }

                    // sleep(size_t seconds, size_t micros) -> time_t current
                    // Sleeps for the given amount of time. Returns the current time. Does not interrupt receive handlers or interrupt handlers. If the sleep time passed in is 0, then the syscall returns immediately.
                    case 7: {
                        uint64_t seconds = trap->xs[REGISTER_A1];
                        uint64_t micros = trap->xs[REGISTER_A2];

                        time_t now = get_time();
                        if (seconds == 0 && micros == 0) {
                            trap->xs[REGISTER_A0] = now.seconds;
                            trap->xs[REGISTER_A1] = now.micros;
                            break;
                        }

                        now.seconds += seconds;
                        now.micros += micros;

                        process_t* process = get_process(trap->pid);
                        process->wake_on_time = now;
                        process->state = PROCESS_STATE_BLOCK_SLEEP;

                        timer_switch(trap);
                        break;
                    }

                    // spawn(void* exe, size_t exe_size, void* args, size_t args_size) -> pid_t child
                    // Spawns a process with the given executable binary. Returns a pid of -1 on failure.
                    // The executable may be a valid elf file. All data will be copied over to a new set of pages.
                    case 8: {
                        void* exe = (void*) trap->xs[REGISTER_A1];
                        size_t exe_size = trap->xs[REGISTER_A2];
                        void* args = (void*) trap->xs[REGISTER_A3];
                        size_t args_size = trap->xs[REGISTER_A4];

                        elf_t elf = verify_elf(exe, exe_size);
                        if (elf.header == NULL) {
                            trap->xs[REGISTER_A0] = 0;
                            break;
                        }

                        pid_t pid = spawn_process_from_elf(trap->pid, &elf, 2, args, args_size);

                        trap->xs[REGISTER_A0] = pid;
                        break;
                    }


                    // kill(pid_t pid) -> int status
                    // Kills the given process. Returns 0 on success, 1 if the process does not exist, and 2 if insufficient permissions.
                    case 9: {
                        pid_t pid = trap->xs[REGISTER_A1];

                        process_t* current = get_process(trap->pid);
                        process_t* victim = get_process(pid);

                        if (victim == NULL) {
                            trap->xs[REGISTER_A0] = 1;
                            break;
                        }

                        if (pid == 0 || (current->user != 0 && victim->user != current->user)) {
                            trap->xs[REGISTER_A0] = 2;
                            break;
                        }

                        kill_process(pid);

                        trap->xs[REGISTER_A0] = 0;
                        break;
                    }

                    // send(bool block, pid_t pid, int type, uint64_t data, uint64_t metadata) -> int status
                    // Sends data to the given process. Returns 0 on success, 1 if process does not exist, 2 if invalid arguments, and 3 if message queue is full. Blocks until the message is sent if block is true. If block is false, then it immediately returns.
                    // Types:
                    // - SIGNAL  - 0
                    //      Metadata can be any integer argument for the signal (for example, the size of the requested data).
                    // - INT     - 1
                    //      Metadata can be set to send a 128 bit integer.
                    // - POINTER - 2
                    //      Metadata contains the size of the pointer. The kernel will share the pages necessary between processes.
                    // - DATA - 3
                    //      Metadata contains the size of the data. The kernel will copy the required data between processes. Maximum is 1 page.
                    case 10: {
                        bool block = trap->xs[REGISTER_A1];
                        pid_t pid = trap->xs[REGISTER_A2];
                        int type = trap->xs[REGISTER_A3];
                        uint64_t data = trap->xs[REGISTER_A4];
                        uint64_t meta = trap->xs[REGISTER_A5];

                        if (get_process(pid) == NULL) {
                            trap->xs[REGISTER_A0] = 1;
                            break;
                        }

                        switch (type) {
                            // Signals
                            case 0:

                            // Integers
                            case 1:
                                break;

                            // Pointers
                            case 2: {
                                if (meta == 0) {
                                    trap->xs[REGISTER_A0] = 2;
                                    return trap;
                                }

                                mmu_level_1_t* current = get_mmu();
                                for (size_t i = 0; i < (meta + PAGE_SIZE - 1) / PAGE_SIZE; i++) {
                                    intptr_t entry = mmu_walk(current, (void*) data + i * PAGE_SIZE);
                                    if ((entry & MMU_BIT_USER) == 0) {
                                        trap->xs[REGISTER_A0] = 2;
                                        return trap;
                                    }
                                }
                                break;
                            }

                            // Data
                            case 3: {
                                if (meta > PAGE_SIZE || meta == 0) {
                                    trap->xs[REGISTER_A0] = 2;
                                    return trap;
                                }

                                void* copy = alloc_pages(1);
                                memcpy(copy, (void*) data, meta);
                                mmu_remove(get_mmu(), copy);
                                mmu_map(get_process(pid)->mmu_data, copy, copy, MMU_BIT_READ | MMU_BIT_WRITE);
                                data = (uint64_t) copy;
                                break;
                            }

                            default:
                                trap->xs[REGISTER_A0] = 2;
                                return trap;
                        }

                        process_message_t message = {
                            .source = trap->pid,
                            .type = type,
                            .data = data,
                            .metadata = meta,
                        };

                        if (!enqueue_message_to_process(pid, message)) {
                            if (block) {
                                trap->pc -= 4;
                                timer_switch(trap);
                            } else trap->xs[REGISTER_A0] = 3;
                        } else trap->xs[REGISTER_A0] = 0;

                        break;
                    }

                    // recv(bool block, pid_t* pid, int* type, uint64_t* data, uint64_t* metadata) -> int status
                    // Blocks until a message is received and deposits the data into the pointers provided. If block is false, then it immediately returns. Returns 0 if message was received and 1 if not.
                    case 11: {
                        bool block = trap->xs[REGISTER_A1];
                        pid_t* pid = (void*) trap->xs[REGISTER_A2];
                        int* type = (void*) trap->xs[REGISTER_A3];
                        uint64_t* data = (void*) trap->xs[REGISTER_A4];
                        uint64_t* meta = (void*) trap->xs[REGISTER_A5];

                        process_message_t message;
                        if (dequeue_message_from_process(trap->pid, &message)) {
                            if (pid != NULL)
                                *pid = message.source;
                            if (type != NULL)
                                *type = message.type;
                            if (data != NULL)
                                *data = message.data;
                            if (meta != NULL)
                                *meta = message.metadata;
                            trap->xs[REGISTER_A0] = 0;

                            if (message.type == 3)
                                mmu_map(get_mmu(), (void*) message.data, (void*) message.data, MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_USER);
                            else if (message.type == 2) {
                                process_t* process = get_process(trap->pid);
                                process_t* sender = get_process(message.source);

                                if (sender == NULL) {
                                    trap->pc -= 4;
                                    timer_switch(trap);
                                    return trap;
                                }

                                mmu_level_1_t* current = get_mmu();
                                mmu_map_mmu(current, sender->mmu_data);
                                for (size_t i = 0; i < (message.metadata + PAGE_SIZE - 1) / PAGE_SIZE; i++) {
                                    void* virtual = (void*) message.data - message.data % PAGE_SIZE + i * PAGE_SIZE;
                                    intptr_t entry = mmu_walk(sender->mmu_data, virtual);
                                    if (entry & MMU_BIT_USER) {
                                        void* physical = MMU_UNWRAP(4, entry);
                                        incr_page_ref_count(physical, 1);
                                        mmu_map(current, process->last_virtual_page + i * PAGE_SIZE, physical, (entry & (MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_EXECUTE)) | MMU_BIT_USER);
                                    }
                                }

                                if (data != NULL)
                                    *data = (uint64_t) process->last_virtual_page + message.data % PAGE_SIZE;
                                remove_mmu_from_mmu(current, sender->mmu_data);
                                process->last_virtual_page += (message.metadata + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;
                            } else if (message.type != 0 && message.type != 1)
                                console_printf("[interrupt_handler] warning: unknown message type 0x%x\n", *type);
                        } else if (block) {
                            trap->pc -= 4;
                            timer_switch(trap);
                        } else trap->xs[REGISTER_A0] = 1;
                        break;
                    }

                    default:
                        console_printf("unknown syscall 0x%lx\n", trap->xs[REGISTER_A0]);
                }
                break;

            // Instruction page fault
            case 12:

            // Load page fault
            case 13:

            // Store page fault
            case 15:

            // Invalid or handled by machine mode
            default:
                while(1);
        }
    }

    return trap;
}
