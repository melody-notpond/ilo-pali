#include "console.h"
#include "interrupt.h"

trap_t* interrupt_handler(uint64_t cause, trap_t* trap) {
    console_printf("cause: %lx\ntrap: %p\n", cause, trap);

    if (cause & 0x8000000000000000) {
        cause &= 0x7fffffffffffffff;

        switch (cause) {
            // Software interrupt
            case 1:

            // Timer interrupt
            case 5:

            // External interrupt
            case 9:
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

            // Environment call (ie, syscall)
            case 8:

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
