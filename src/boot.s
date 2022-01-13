.section .text
.global _start

# a0 - current hart id
# a1 - pointer to flattened device tree

_start:
	# Initialise stack pointer
	la sp, stack_top
	mv fp, sp

    # Init interrupt handler
    la t0, handle_interrupt
    csrw stvec, t0

    # Jump to init
	j kinit

finish:
	j finish

