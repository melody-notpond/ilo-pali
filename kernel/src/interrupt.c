#include <stddef.h>

#include "console.h"
#include "interrupt.h"
#include "memory.h"
#include "opensbi.h"
#include "process.h"
#include "string.h"
#include "time.h"

static void* plic_base;
static size_t plic_len;

#define MAX_INTERRUPT   64
struct interrupt_list {
    struct interrupt_list* next;
    capability_t cap;
    pid_t pid;
} *interrupt_subscribers[MAX_INTERRUPT] = { NULL };
atomic_bool interrupt_subscribers_lock = false;

#define CONTEXT(hartid, machine) (2*hartid+machine)

extern void hart_suspend_resume(uint64_t hartid, trap_t* trap);

const int PROCESS_QUANTUM = 10000;

// timer_switch(trap_t*) -> void
// Switches to a new process, or suspends the hart if no process is available.
void timer_switch(trap_t* trap) {
    pid_t next_pid = get_next_waiting_process(trap->pid);
    if (next_pid != (pid_t) -1)
        switch_to_process(trap, next_pid);
    else {
        save_process(trap);
        trap->pid = -1;
    }

    time_t next = get_time();
    next.micros += PROCESS_QUANTUM;
    while (next.micros >= 1000000) {
        next.seconds += 1;
        next.micros -= 1000000;
    }

    set_next_time_interrupt(next);
    if (next_pid == (pid_t) -1) {
        sbi_hart_suspend(0, (unsigned long) hart_suspend_resume, (unsigned long) trap);

        // In the event that suspending doesn't work, just loop forever until an interrupt occurs
        uint64_t sstatus;
        asm volatile("csrr %0, sstatus" : "=r" (sstatus));
        sstatus |= 1 << 8 | 1 << 5;
        uint64_t s = sstatus;
        asm volatile("csrw sstatus, %0" : "=r" (s));
        extern void do_nothing();
        trap->pc = (uint64_t) do_nothing;
        jump_out_of_trap(trap);
    } else {
        uint64_t sstatus;
        asm volatile("csrr %0, sstatus" : "=r" (sstatus));
        sstatus &= ~(1 << 8 | 1 << 5);
        uint64_t s = sstatus;
        asm volatile("csrw sstatus, %0" : "=r" (s));
    }
}

// init_interrupts(uint64_t, fdt_t*) -> void
// Inits interrupts.
void init_interrupts(uint64_t hartid, fdt_t* fdt) {
    void* node = fdt_find(fdt, "plic", NULL);
    struct fdt_property reg = fdt_get_property(fdt, node, "reg");

    // TODO: use #address-cells and #size-cells
    plic_base = kernel_space_phys2virtual((void*) be_to_le(64, reg.data));
    plic_len = be_to_le(64, reg.data + 8);

    for (size_t i = 1; i <= MAX_INTERRUPT; i++) {
        ((uint32_t*) plic_base)[i] = 0xffffffff;
    }

    // TODO: spread interrupts across harts
    for (size_t i = 0; i < 1024 / 4; i++) {
        ((uint32_t*) (plic_base + 0x002000 + 0x80 * CONTEXT(hartid, 1)))[i] = 0xffffffff;
    }

    *(uint32_t*) (plic_base + 0x200000 + 0x1000 * CONTEXT(hartid, 1)) = 0;
}

trap_t* interrupt_handler(uint64_t cause, trap_t* trap) {
    if (cause & 0x8000000000000000) {
        cause &= 0x7fffffffffffffff;

        switch (cause) {
            // Software interrupt (interprocessor interrupt, indicating that a process has died and each hart should check if its process is still alive)
            case 1: {
                uint64_t sip = 0;
                asm volatile("csrw sip, %0" : "=r" (sip));
                if (!process_exists(trap->pid)) {
                    kill_process(trap->pid);
                    timer_switch(trap);
                }
                break;
            }

            // Timer interrupt
            case 5:
                timer_switch(trap);
                break;

            // External interrupt
            case 9: {
                volatile uint32_t* claim = (volatile uint32_t*) (plic_base + 0x200004 + 0x1000 * CONTEXT(trap->hartid, 1));
                uint32_t interrupt_id = *claim;

                if (interrupt_id != 0 && interrupt_id <= MAX_INTERRUPT) {
                    bool f = false;
                    while (!atomic_compare_exchange_weak(&interrupt_subscribers_lock, &f, true)) {
                        f = false;
                    }

                    struct interrupt_list* list = interrupt_subscribers[interrupt_id - 1];
                    while (list) {
                        // TODO: queue interrupt to channel
                        list = list->next;
                    }

                    interrupt_subscribers_lock = false;
                }

                *claim = interrupt_id;
                break;
            }

            // Everything else is either invalid or handled by the firmware.
            default:
                console_printf("async cause: %lx\ntrap location: %lx\ntrap caller: %lx\n", cause, trap->pc, trap->xs[REGISTER_RA]);
                while(1);
                break;
        }
    } else {
        switch (cause) {
            // Breakpoint
            // TODO: figure out how this works
            case 3:
                console_printf("breakpoint\n");
                break;

            // Environment call (ie, syscall)
            case 8:
                trap->pc += 4;
                switch (trap->xs[REGISTER_A0]) {
                    // uart_puts(char* message) -> void
                    // Writes a message to the UART port.
                    case 0: {
                        char* message = (char*) trap->xs[REGISTER_A1];
                        process_t* process = get_process(trap->pid);
                        console_printf("[PID 0x%lx (%s) @ hartid 0x%lx] %s\n", trap->pid, process->name, trap->hartid, message);
                        unlock_process(process);
                        break;
                    }

                    // page_alloc(size_t page_count, int permissions) -> void*
                    // Allocates a page with the given permissions.
                    case 1: {
                        size_t page_count = trap->xs[REGISTER_A1];
                        int permissions = trap->xs[REGISTER_A2];

                        int perms = 0;
                        if (permissions & 2)
                            perms |= MMU_BIT_WRITE;
                        else if (permissions & 1)
                            perms |= MMU_BIT_EXECUTE;
                        if (permissions & 4)
                            perms |= MMU_BIT_READ;

                        mmu_level_1_t* mmu = get_mmu();
                        process_t* process = get_process(trap->pid);
                        void* result = process->last_virtual_page;
                        for (size_t i = 0; i < page_count; i++) {
                            mmu_alloc(mmu, process->last_virtual_page, perms | MMU_BIT_VALID | MMU_BIT_USER);
                            process->last_virtual_page += PAGE_SIZE;
                        }
                        unlock_process(process);
                        trap->xs[REGISTER_A0] = (uint64_t) result;
                        break;
                    }

                    // page_perms(void* page, size_t page_count, int permissions) -> int
                    // Changes the page's permissions. Returns 0 if successful and 1 if not.
                    case 2: {
                        void* page = (void*) trap->xs[REGISTER_A1];
                        size_t page_count = trap->xs[REGISTER_A2];
                        int permissions = trap->xs[REGISTER_A3];

                        int perms = 0;
                        if (permissions & 2)
                            perms |= MMU_BIT_WRITE;
                        else if (permissions & 1)
                            perms |= MMU_BIT_EXECUTE;
                        if (permissions & 4)
                            perms |= MMU_BIT_READ;

                        mmu_level_1_t* mmu = get_mmu();
                        for (size_t i = 0; i < page_count; i++) {
                            if ((mmu_walk(mmu, page + i * PAGE_SIZE) & (MMU_BIT_USER | MMU_BIT_VALID)) != (MMU_BIT_USER | MMU_BIT_VALID)) {
                                trap->xs[REGISTER_A0] = 1;
                                return trap;
                            }
                        }
                        for (size_t i = 0; i < page_count; i++) {
                            mmu_change_flags(mmu, page + PAGE_SIZE * i, perms | MMU_BIT_VALID | MMU_BIT_USER);
                        }
                        trap->xs[REGISTER_A0] = 0;
                        break;
                    }

                    // page_dealloc(void* page, size_t page_count) -> int
                    // Deallocates a page. Returns 0 if successful and 1 if not.
                    case 3: {
                        void* page = (void*) trap->xs[REGISTER_A1];
                        size_t page_count = trap->xs[REGISTER_A2];
                        mmu_level_1_t* mmu = get_mmu();
                        for (size_t i = 0; i < page_count; i++) {
                            if ((mmu_walk(mmu, page + i * PAGE_SIZE) & (MMU_BIT_USER | MMU_BIT_VALID)) != (MMU_BIT_USER | MMU_BIT_VALID)) {
                                trap->xs[REGISTER_A0] = 1;
                                return trap;
                            }
                        }
                        for (size_t i = 0; i < page_count; i++) {
                            dealloc_pages(mmu_remove(mmu, page + i * PAGE_SIZE), i);
                        }
                        trap->xs[REGISTER_A0] = 0;
                        break;
                    }

                    // sleep(uint64_t seconds, uint64_t micros) -> void
                    // Sleeps for the given amount of time.
                    case 4: {
                        uint64_t seconds = trap->xs[REGISTER_A1];
                        uint64_t micros = trap->xs[REGISTER_A2];
                        if (seconds == 0 && micros == 0) {
                            break;
                        }

                        time_t now = get_time();
                        micros += now.micros;
                        seconds += now.seconds;
                        if (micros >= 1000000) {
                            micros -= 1000000;
                            seconds += 1;
                        }

                        process_t* process = get_process(trap->pid);
                        process->wake_on_time = (time_t) {
                            .seconds = seconds,
                            .micros = micros,
                        };
                        process->state = PROCESS_STATE_BLOCK_SLEEP;
                        unlock_process(process);
                        timer_switch(trap);
                        break;
                    }

                    // spawn(void* elf, size_t elf_size, char* name, size_t argc, char** argv) -> pid_t
                    // Spawns a new process. Returns -1 on error.
                    case 5: {
                        void* elf_raw = (void*) trap->xs[REGISTER_A1];
                        size_t elf_size = trap->xs[REGISTER_A2];
                        char* name = (char*) trap->xs[REGISTER_A3];
                        size_t argc = trap->xs[REGISTER_A4];
                        char** argv = (char**) trap->xs[REGISTER_A5];
                        elf_t elf = verify_elf(elf_raw, elf_size);
                        if (elf.header == NULL) {
                            trap->xs[REGISTER_A0] = -1;
                            break;
                        }

                        process_t* process = spawn_process_from_elf(name, strlen(name), &elf, 2, argc, argv);
                        trap->xs[REGISTER_A0] = process->pid;
                        unlock_process(process);
                        break;
                    }

                    // spawn_thread(void (*func)(void* data), void* data) -> pid_t
                    // Spawns a new process in the same address space, executing the given function.
                    case 6: {
                        void* func = (void*) trap->xs[REGISTER_A1];
                        void* data = (void*) trap->xs[REGISTER_A2];
                        process_t* thread = spawn_thread_from_func(trap->pid, func, 2, data);
                        trap->xs[REGISTER_A0] = thread->pid;
                        unlock_process(thread);
                        break;
                    }

                    // exit(int64_t code) -> !
                    // Exits the current process.
                    case 7: {
                        int64_t code = trap->xs[REGISTER_A1];
                        (void) code; // TODO: use this
                        kill_process(trap->pid);
                        timer_switch(trap);
                        break;
                    }

                    // get_allowed_memory(size_t i, struct allowed_memory* memory) -> bool
                    // Gets an element of the allowed memory list. Returns true if the given index exists and false if out of bounds.
                    //
                    // The struct is defined below:
                    // struct allowed_memory {
                    //      char name[16];
                    //      void* start;
                    //      size_t size;
                    // };
                    case 8: {
                        size_t i = trap->xs[REGISTER_A1];
                        struct allowed_memory* memory = (struct allowed_memory*) trap->xs[REGISTER_A2];
                        process_t* process = get_process(trap->pid);
                        if (i < PROCESS_MAX_ALLOWED_MEMORY_RANGES && process->allowed_memory_ranges[i].size != 0) {
                            *memory = process->allowed_memory_ranges[i];
                            trap->xs[REGISTER_A0] = true;
                        } else trap->xs[REGISTER_A0] = false;
                        unlock_process(process);
                        break;
                    }

                    default:
                        console_printf("unknown syscall 0x%lx\n", trap->xs[REGISTER_A0]);
                        break;
                }
                break;

            // Instruction address misaligned
            case 0:

            // Instruction access fault
            case 1:

            // Illegal instruction
            case 2:

            // Load address misaligned
            case 4:

            // Load access fault
            case 5:

            // Store address misaligned
            case 6:

            // Store access fault
            case 7:

            // Instruction page fault
            case 12:

            // Load page fault
            case 13:

            // Store page fault
            case 15: {
                process_t* process = get_process_unsafe(trap->pid);
                if (process->faulted || process->fault_handler == NULL) {
                    console_clear_lock_unsafe();
                    console_printf("cause: %lx\ntrap location: %lx\ntrap caller: %lx\ntrap process: %lx (%s)\n", cause, trap->pc, trap->xs[REGISTER_RA], trap->pid, process->name);
                    unlock_process(process);
                    // TODO: send segfault message
                    if (trap->pid != 0) {
                        kill_process(trap->pid);
                        timer_switch(trap);
                        return trap;
                    }

                    // TODO: indicate to other harts that kernel has panicked
                    console_printf("KERNEL PANIC! INITD FAULTED!\n");
                    while(1);
                }

                process->faulted = true;
                trap->xs[REGISTER_A0] = cause;
                trap->xs[REGISTER_A1] = trap->pc;
                trap->xs[REGISTER_A2] = trap->xs[REGISTER_SP];
                trap->xs[REGISTER_A3] = trap->xs[REGISTER_FP];
                trap->pc = (uint64_t) process->fault_handler;
                trap->xs[REGISTER_SP] = (uint64_t) process->fault_stack - 8;
                trap->xs[REGISTER_FP] = (uint64_t) process->fault_stack - 8;
                unlock_process(process);
                break;
            }

            // Invalid or handled by machine mode
            default: {
                process_t* process = get_process(trap->pid);
                console_printf("cause: %lx\ntrap location: %lx\ntrap caller: %lx\ntrap process: %lx (%s)\n", cause, trap->pc, trap->xs[REGISTER_RA], trap->pid, process->name);
                unlock_process(process);
                while(1);
            }
        }
    }

    return trap;
}
