#include <stddef.h>

#include "fdt.h"
#include "iter/string.h"
#include "syscalls.h"
#include "fat16.h"

void _start(void* fdt) {
    uart_write("initd started\n", 14);

    fdt_t tree = verify_fdt(fdt);

    void* chosen = fdt_path(&tree, "/chosen", NULL);
    struct fdt_property initrd_start_prop = fdt_get_property(&tree, chosen, "linux,initrd-start");
    void* initrd_start = (void*) be_to_le(initrd_start_prop.len * 8, initrd_start_prop.data);
    struct fdt_property initrd_end_prop = fdt_get_property(&tree, chosen, "linux,initrd-end");
    void* initrd_end = (void*) be_to_le(initrd_end_prop.len * 8, initrd_end_prop.data);

    fat16_fs_t initrd = verify_initrd(initrd_start, initrd_end);
    size_t len;
    void* bytes = read_file_full(&initrd, "maps", &len);
    str_t module_maps = {
        .len = len,
        .bytes = bytes,
    };

    for (str_t part = str_split(module_maps, S("\n"), STREMPTY); part.bytes != NULL; part = str_split(module_maps, S("\n"), part)) {
        str_t device = str_split(part, S(" "), STREMPTY);
        str_t module = str_split(part, S(" "), device);
        uart_write((void*) device.bytes, device.len);
        uart_write(" maps to ", 9);
        uart_write((void*) module.bytes, module.len);
        uart_write("\n", 1);
        void* module_elf = NULL;
        size_t module_size = 0;

        void* node = NULL;
        while ((node = fdt_find(&tree, device, node))) {
            uart_write("found thing\n", 12);
            if (module_elf == NULL) {
                char buffer[module.len + 1];
                memcpy(buffer, module.bytes, module.len);
                buffer[module.len] = '\0';

                module_elf = read_file_full(&initrd, buffer, &module_size);
                if (module_elf == NULL)
                    break;
            }

            uart_write("spawning\n", 9);
            spawn_process(module_elf, module_size, NULL, 0);
        }

        dealloc_page(module_elf, (module_size + PAGE_SIZE - 1) / PAGE_SIZE);
    }

    while(1);
}

