#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "fdt.h"
#include <stdbool.h>
#include <stdint.h>

#define REGISTER_ZERO   0
#define REGISTER_RA     1
#define REGISTER_SP     2
#define REGISTER_GP     3
#define REGISTER_TP     4
#define REGISTER_T0     5
#define REGISTER_T1     6
#define REGISTER_T2     7
#define REGISTER_FP     8
#define REGISTER_S1     9
#define REGISTER_A0    10
#define REGISTER_A1    11
#define REGISTER_A2    12
#define REGISTER_A3    13
#define REGISTER_A4    14
#define REGISTER_A5    15
#define REGISTER_A6    16
#define REGISTER_A7    17
#define REGISTER_S2    18
#define REGISTER_S3    19
#define REGISTER_S4    20
#define REGISTER_S5    21
#define REGISTER_S6    22
#define REGISTER_S7    23
#define REGISTER_S8    24
#define REGISTER_S9    25
#define REGISTER_S10   26
#define REGISTER_S11   27
#define REGISTER_T3    28
#define REGISTER_T4    29
#define REGISTER_T5    30
#define REGISTER_T6    31

typedef uint64_t pid_t;

typedef struct {
    uint64_t xs[32];
    uint64_t pc;
    uint64_t interrupt_stack;
    double fs[32];

    uint64_t hartid;
    pid_t pid;
} trap_t;

// timer_switch(trap_t*) -> void
// Switches to a new process, or suspends the hart if no process is available.
void timer_switch(trap_t* trap);

// init_interrupts(uint64_t, fdt_t*) -> void
// Inits interrupts.
void init_interrupts(uint64_t hartid, fdt_t* fdt);

// jump_out_of_trap(trap_t*) -> void
// Jumps out of a trap.
void jump_out_of_trap(trap_t* trap);

#endif /* INTERRUPT_H */
