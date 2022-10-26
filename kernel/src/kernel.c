#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "elf.h"
#include "fdt.h"
#include "fat16.h"
#include "interrupt.h"
#include "memory.h"
#include "mmu.h"
#include "opensbi.h"
#include "process.h"

#define MAX_TRAP_COUNT 64
#define STACK_SIZE     0x8000

trap_t traps[MAX_TRAP_COUNT];
size_t cpu_count = 0;

void init_hart_helper(uint64_t hartid, mmu_level_1_t* mmu) {
    extern int stack_top;
    trap_t* trap = &traps[hartid];
    trap->hartid = hartid;
    trap->interrupt_stack = (uint64_t) &stack_top - STACK_SIZE * hartid;
    trap->pid = -1;

    trap_t* t = trap;
    asm volatile("csrw sscratch, %0": "=r" (t));

    uint64_t sstatus;
    asm volatile("csrr %0, sstatus" : "=r" (sstatus));
    sstatus |= 1 << 18 | 1 << 8 | 1 << 5;
    uint64_t s = sstatus;
    asm volatile("csrw sstatus, %0" : "=r" (s));

    uint64_t sie = 0x222;
    asm volatile("csrw sie, %0" : "=r" (sie));

    extern void do_nothing();

    set_mmu(mmu);
    trap->pc = (uint64_t) do_nothing;
    sbi_set_timer(0);
    jump_out_of_trap(trap);
}

void kinit(uint64_t hartid, void* fdt) {
    console_printf("[kinit] toki, ale o!\n[kinit] hartid: %lx\n[kinit] fdt pointer: %p\n", hartid, fdt);

    fdt_t devicetree = verify_fdt(fdt);
    if (devicetree.header == NULL) {
        console_printf("[kinit] invalid fdt pointer %p\n", fdt);
        while(1);
    }

    dump_fdt(&devicetree, NULL);

    void* last = NULL;
    while ((last = fdt_find(&devicetree, "cpu", last))) {
        cpu_count++;
    }

    console_printf("[kinit] %lx cpus present\n", cpu_count);

    init_time(&devicetree);
    init_pages(&devicetree);
    mark_as_used(&devicetree, (be_to_le(32, devicetree.header->totalsize) + PAGE_SIZE - 1) / PAGE_SIZE);

    void* chosen = fdt_path(&devicetree, "/chosen", NULL);
    struct fdt_property initrd_start_prop = fdt_get_property(&devicetree, chosen, "linux,initrd-start");
    void* initrd_start = (void*) be_to_le(32, initrd_start_prop.data);

    struct fdt_property initrd_end_prop = fdt_get_property(&devicetree, chosen, "linux,initrd-end");
    void* initrd_end = (void*) be_to_le(32, initrd_end_prop.data);

    mark_as_used(initrd_start, (initrd_end - initrd_start + PAGE_SIZE - 1) / PAGE_SIZE);
    mmu_level_1_t* top = create_mmu_table();
    identity_map_kernel(top, &devicetree, initrd_start, initrd_end);
    set_mmu(top);

    console_printf("[kinit] initrd start: %p\n[kinit] initrd end: %p\n", initrd_start, initrd_end);

    init_interrupts(hartid, &devicetree);

    fat16_fs_t fat = verify_initrd(initrd_start, initrd_end);
    if (fat.fat == NULL) {
        console_puts("[kinit] initrd image is invalid\n");
        while(1);
    }

    console_puts("[kinit] verified initrd image\n");
    init_processes();

    size_t size;
    void* data = read_file_full(&fat, "initd", &size);
    elf_t elf = verify_elf(data, size);
    if (elf.header == NULL) {
        console_puts("[kinit] failed to verify initd elf file\n");
        while(1);
    }

    process_t* initd = spawn_process_from_elf("initd", 5, &elf, 2, 3, (char* []) { "uwu", "owo", "nya" });
    free(data);

    for (page_t* p = initrd_start; p < (page_t*) (initrd_end + PAGE_SIZE - 1); p++) {
        mmu_change_flags(top, p, MMU_BIT_READ | MMU_BIT_USER);
    }

    for (page_t* p = (page_t*) devicetree.header; p < (page_t*) ((void*) devicetree.header + be_to_le(32, devicetree.header->totalsize) + PAGE_SIZE - 1); p++) {
        mmu_change_flags(top, p, MMU_BIT_READ | MMU_BIT_USER);
    }

    // initd->xs[REGISTER_A0] = (uint64_t) fdt;
    console_printf("argc = %lx, argv = %p\n", initd->xs[REGISTER_A0], (void*) initd->xs[REGISTER_A1]);
    mmu_level_1_t* mmu = initd->mmu_data;
    unlock_process(initd);

    console_puts("[kinit] initialising harts\n");
    extern void init_hart(uint64_t hartid, mmu_level_1_t* mmu);
    for (size_t i = 0; i < cpu_count; i++) {
        if (i != hartid) {
            sbi_hart_start(i, init_hart, (uint64_t) mmu);
        }
    }

    init_hart_helper(hartid, initd->mmu_data);

    while(1);
}

