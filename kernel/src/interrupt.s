.global handle_interrupt
.global jump_out_of_trap

/*
typedef struct {
    uint64_t xs[32];
    uint64_t pc;
    uint64_t interrupt_stack;
    double fs[32];

    uint64_t hartid;
    pid_t pid;
} trap_t;
*/
handle_interrupt:
    # Save registers
    csrrw t6, sscratch, t6
    sd x0,  0x000(t6)
    sd x1,  0x008(t6)
    sd x2,  0x010(t6)
    sd x3,  0x018(t6)
    sd x4,  0x020(t6)
    sd x5,  0x028(t6)
    sd x6,  0x030(t6)
    sd x7,  0x038(t6)
    sd x8,  0x040(t6)
    sd x9,  0x048(t6)
    sd x10, 0x050(t6)
    sd x11, 0x058(t6)
    sd x12, 0x060(t6)
    sd x13, 0x068(t6)
    sd x14, 0x070(t6)
    sd x15, 0x078(t6)
    sd x16, 0x080(t6)
    sd x17, 0x088(t6)
    sd x18, 0x090(t6)
    sd x19, 0x098(t6)
    sd x20, 0x0a0(t6)
    sd x21, 0x0a8(t6)
    sd x22, 0x0b0(t6)
    sd x23, 0x0b8(t6)
    sd x24, 0x0c0(t6)
    sd x25, 0x0c8(t6)
    sd x26, 0x0d0(t6)
    sd x27, 0x0d8(t6)
    sd x28, 0x0e0(t6)
    sd x29, 0x0e8(t6)
    sd x30, 0x0f0(t6)

    # Save t6
    csrr t5, sscratch
    sd t5, 0x0f8(t6)
    csrw sscratch, t6

    # Save pc
    csrr t5, sepc
    sd t5, 0x100(t6)

    # Set sp
    ld sp, 0x108(t6)

    # Call interrupt handler
    csrr a0, scause
    mv a1, t6
    jal interrupt_handler
    mv t6, a0

jump_out_of_trap:
    # Revert pc
    mv t6, a0
    ld t5, 0x100(t6)
    csrw sepc, t5

    # Revert registers
    ld x0,  0x000(t6)
    ld x1,  0x008(t6)
    ld x2,  0x010(t6)
    ld x3,  0x018(t6)
    ld x4,  0x020(t6)
    ld x5,  0x028(t6)
    ld x6,  0x030(t6)
    ld x7,  0x038(t6)
    ld x8,  0x040(t6)
    ld x9,  0x048(t6)
    ld x10, 0x050(t6)
    ld x11, 0x058(t6)
    ld x12, 0x060(t6)
    ld x13, 0x068(t6)
    ld x14, 0x070(t6)
    ld x15, 0x078(t6)
    ld x16, 0x080(t6)
    ld x17, 0x088(t6)
    ld x18, 0x090(t6)
    ld x19, 0x098(t6)
    ld x20, 0x0a0(t6)
    ld x21, 0x0a8(t6)
    ld x22, 0x0b0(t6)
    ld x23, 0x0b8(t6)
    ld x24, 0x0c0(t6)
    ld x25, 0x0c8(t6)
    ld x26, 0x0d0(t6)
    ld x27, 0x0d8(t6)
    ld x28, 0x0e0(t6)
    ld x29, 0x0e8(t6)
    ld x30, 0x0f0(t6)
    ld x31, 0x0f8(t6)
    sret

