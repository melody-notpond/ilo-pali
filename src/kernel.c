#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "fdt.h"
#include "fat16.h"
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

    void* chosen = fdt_path(&devicetree, "/chosen", NULL);
    struct fdt_property initrd_start_prop = fdt_get_property(&devicetree, chosen, "linux,initrd-start");
    void* initrd_start = (void*) be_to_le(32, initrd_start_prop.data);

    struct fdt_property initrd_end_prop = fdt_get_property(&devicetree, chosen, "linux,initrd-end");
    void* initrd_end = (void*) be_to_le(32, initrd_end_prop.data);

    console_printf("initrd start: %p\ninitrd end: %p\n", initrd_start, initrd_end);

    fat16_fs_t fat = verify_initrd(initrd_start, initrd_end);
    fat_root_dir_entry_t* initd = find_file_in_root_directory(&fat, "initd");
    void* data = get_fat_cluster_data(&fat, initd->file.first_cluster_low);
    console_put_hexdump(data, 16);

	while(1);
}

