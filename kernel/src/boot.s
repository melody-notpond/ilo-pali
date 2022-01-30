.section .text
.global _start
.global init_hart
.global do_nothing

# a0 - current hart id
# a1 - pointer to flattened device tree

_start:
	# Initialise stack pointer
	la sp, stack_top
    li t0, 0x8000
    mul t0, t0, a0
    sub sp, sp, t0
	mv fp, sp

    # Init gp, whatever that is
    .option push
    .option norelax
    lla gp, __global_pointer$
    .option pop

    # Init interrupt handler
    la t0, handle_interrupt
    csrw stvec, t0

    # Jump to init
	j kinit

init_hart:
	# Initialise stack pointer
	la sp, stack_top
    li t0, 0x8000
    mul t0, t0, a0
    sub sp, sp, t0
	mv fp, sp

    # Init gp, whatever that is
    .option push
    .option norelax
    lla gp, __global_pointer$
    .option pop

    # Init interrupt handler
    la t0, handle_interrupt
    csrw stvec, t0

    # Jump to init
    li a2, 0
	j init_hart_helper

do_nothing:
    j do_nothing
