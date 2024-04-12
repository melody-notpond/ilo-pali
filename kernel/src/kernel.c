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

#define STACK_SIZE     0x8000

void init_hart_helper(uint64_t hartid, struct mmu_root mmu) {
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

    // dump_fdt(&devicetree, NULL);

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
    struct mmu_root top = create_mmu_table();
    set_mmu(top);

    fdt_phys2safe(&devicetree);
    initrd_start = phys2safe(initrd_start);
    initrd_end = phys2safe(initrd_end);

    console_printf("[kinit] initrd start: %p\n[kinit] initrd end: %p\n", initrd_start, initrd_end);

    init_interrupts(hartid, &devicetree);

    fat16_fs_t fat = verify_initrd(initrd_start, initrd_end);
    if (fat.fat == NULL) {
        console_puts("[kinit] initrd image is invalid\n");
        while(1);
    }

    console_puts("[kinit] verified initrd image\n");
    init_processes(64); // TODO: configure this

    size_t size;
    void* data = read_file_full(&fat, "initd", &size);
    elf_t elf = verify_elf(data, size);
    if (elf.header == NULL) {
        console_puts("[kinit] failed to verify initd elf file\n");
        while(1);
    }

    struct s_task *initd = spawn_task_from_elf("initd", 5, &elf, 2, 0, NULL);
    free(data);

    struct mmu_entry *entry = mmu_walk_to_entry(initd->mmu_data, (void *) 0x11a88);
    if (entry)
        console_printf("entry for 0x11a88 points to %p and has flags %x\n", mmu_entry_phys(*entry), mmu_entry_flags(*entry, MMU_ALL_BITS));
    else console_printf("entry for 0x11a88 doesnt exist!\n");


    struct mmu_root mmu = initd->mmu_data;

    console_puts("[kinit] initialising harts\n");
    extern void init_hart(uint64_t hartid, struct mmu_root mmu);
    for (size_t i = 0; i < cpu_count; i++) {
        if (i != hartid) {
            sbi_hart_start(i, init_hart, (uint64_t) mmu.data);
        }
    }

    init_hart_helper(hartid, initd->mmu_data);

    while(1);
}
