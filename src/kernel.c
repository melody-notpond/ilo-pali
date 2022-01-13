#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "elf.h"
#include "fdt.h"
#include "fat16.h"
#include "interrupt.h"
#include "memory.h"
#include "mmu.h"

trap_t trap = { 0 };
extern int stack_start;

void kinit(uint64_t hartid, void* fdt) {
    trap.hartid = hartid;
    trap.xs[REGISTER_SP] = (uint64_t) &stack_start;
    trap.xs[REGISTER_FP] = trap.xs[REGISTER_SP];

    console_printf("toki, ale o!\nhartid: %lx\nfdt pointer: %p\n", hartid, fdt);

    fdt_t devicetree = verify_fdt(fdt);
    if (devicetree.header == NULL) {
        console_printf("Invalid fdt pointer %p\n", fdt);
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

    console_printf("initrd start: %p\ninitrd end: %p\n", initrd_start, initrd_end);
    mark_as_used(initrd_start, (intptr_t) (initrd_end - initrd_start + PAGE_SIZE - 1) / PAGE_SIZE);

    /*
    mmu_level_1_t* top = create_mmu_table();
    identity_map_kernel(top, &devicetree, initrd_start, initrd_end);
    set_mmu(top);
    */

    fat16_fs_t fat = verify_initrd(initrd_start, initrd_end);
    size_t size;
    void* data = read_file_full(&fat, "initd", &size);
    console_put_hexdump(data, size);
    verify_elf(data, size);

	while(1);
}

