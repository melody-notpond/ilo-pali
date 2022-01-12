#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "fdt.h"

void kinit(uint64_t hartid, void* fdt) {
    console_printf("toki, %s!\n%x\n", "ale o", 0x123abc);

    console_printf("hartid: %lx\nfdt pointer: %p\n", hartid, fdt);
    fdt_t devicetree = verify_fdt(fdt);
    dump_fdt(&devicetree, NULL);

	while(1);
}

