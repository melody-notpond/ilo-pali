#include <stddef.h>

#include "console.h"
#include "interrupt.h"
#include "memory.h"
#include "process.h"
#include "opensbi.h"

trap_t* interrupt_handler(uint64_t cause, trap_t* trap) {
    //console_printf("cause: %lx\ntrap location: %lx\n", cause, trap->pc);

    if (cause & 0x8000000000000000) {
        cause &= 0x7fffffffffffffff;

        switch (cause) {
            // Software interrupt
            case 1:
                while(1);

            // Timer interrupt
            case 5: {
                pid_t next_pid = get_next_waiting_process(trap->pid);
                switch_to_process(trap, next_pid);
                uint64_t time = 0;
                asm volatile("csrr %0, time" : "=r" (time));
                sbi_set_timer(time + 10000);
                break;
            }

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
                        } else {
                            // TODO
                            trap->xs[REGISTER_A0] = 0;
                        }

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
                                mmu_remove(process->mmu_data, addr + i * PAGE_SIZE);
                            } else {
                                trap->xs[REGISTER_A0] = 1;
                                return trap;
                            }
                        }

                        trap->xs[REGISTER_A0] = 0;
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
