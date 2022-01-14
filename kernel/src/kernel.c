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

trap_t trap = { 0 };

void kinit(uint64_t hartid, void* fdt) {
    extern int stack_top;
    trap.hartid = hartid;
    trap.interrupt_stack = (uint64_t) &stack_top;

    trap_t* current_trap = &trap;
    asm volatile("csrw sscratch, %0" : "=r" (current_trap));

    console_printf("toki, ale o!\nhartid: %lx\nfdt pointer: %p\n", hartid, fdt);

    fdt_t devicetree = verify_fdt(fdt);
    if (devicetree.header == NULL) {
        console_printf("invalid fdt pointer %p\n", fdt);
        while(1);
    }

    dump_fdt(&devicetree, NULL);

    init_pages(&devicetree);
    mark_as_used(&devicetree, (be_to_le(32, devicetree.header->totalsize) + PAGE_SIZE - 1) / PAGE_SIZE);

    void* chosen = fdt_path(&devicetree, "/chosen", NULL);
    struct fdt_property initrd_start_prop = fdt_get_property(&devicetree, chosen, "linux,initrd-start");
    void* initrd_start = (void*) be_to_le(32, initrd_start_prop.data);

    struct fdt_property initrd_end_prop = fdt_get_property(&devicetree, chosen, "linux,initrd-end");
    void* initrd_end = (void*) be_to_le(32, initrd_end_prop.data);

    mmu_level_1_t* top = create_mmu_table();
    identity_map_kernel(top, &devicetree, initrd_start, initrd_end);
    set_mmu(top);

    console_printf("initrd start: %p\ninitrd end: %p\n", initrd_start, initrd_end);
    mark_as_used(initrd_start, (intptr_t) (initrd_end - initrd_start + PAGE_SIZE - 1) / PAGE_SIZE);

    fat16_fs_t fat = verify_initrd(initrd_start, initrd_end);
    if (fat.fat == NULL) {
        console_puts("initrd image is invalid\n");
        while(1);
    }

    console_puts("verified initrd image\n");
    init_processes();

    {
        size_t size;
        void* data = read_file_full(&fat, "a", &size);
        elf_t elf = verify_elf(data, size);
        if (elf.header == NULL) {
            console_puts("failed to verify initd elf file\n");
            while(1);
        }

        spawn_process_from_elf(0, &elf, 2);
        dealloc_pages(data, (size + PAGE_SIZE - 1) / PAGE_SIZE);
    }

    {
        size_t size;
        void* data = read_file_full(&fat, "b", &size);
        elf_t elf = verify_elf(data, size);
        if (elf.header == NULL) {
            console_puts("failed to verify initd elf file\n");
            while(1);
        }

        spawn_process_from_elf(0, &elf, 2);
        dealloc_pages(data, (size + PAGE_SIZE - 1) / PAGE_SIZE);
    }

    for (page_t* p = initrd_start; p < (page_t*) (initrd_end + PAGE_SIZE - 1); p++) {
        mmu_change_flags(top, p, MMU_BIT_READ | MMU_BIT_USER);
    }

    for (page_t* p = (page_t*) devicetree.header; p < (page_t*) ((void*) devicetree.header + be_to_le(32, devicetree.header->totalsize) + PAGE_SIZE - 1); p++) {
        mmu_change_flags(top, p, MMU_BIT_READ | MMU_BIT_USER);
    }

    uint64_t sstatus;
    asm volatile("csrr %0, sstatus" : "=r" (sstatus));
    sstatus |= 1 << 18;
    asm volatile("csrw sstatus, %0" : "=r" (sstatus));

    uint64_t sie = 0x220;
    asm volatile("csrw sie, %0" : "=r" (sie));

    console_puts("starting initd\n");
    switch_to_process(&trap, 0);
    sbi_set_timer(0);
    jump_out_of_trap(&trap);

	while(1);
}

