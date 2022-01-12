#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "fdt.h"
#include "memory.h"

void kinit(uint64_t hartid, void* fdt) {
    console_printf("toki, ale o!\nhartid: %lx\nfdt pointer: %p\n", hartid, fdt);

    fdt_t devicetree = verify_fdt(fdt);
    if (devicetree.header == NULL) {
        console_printf("Invalid fdt pointer %p\n", fdt);
        while(1);
    }

    dump_fdt(&devicetree, NULL);

    init_pages(&devicetree);

    void* page = alloc_pages(2);
    console_printf("page %p has %x references\n", page, *page_ref_count(page));
    dealloc_pages(page, 2);
    console_printf("page %p has %x references\n", page, *page_ref_count(page));

	while(1);
}

