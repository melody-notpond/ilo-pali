#include <stddef.h>

#include "fdt.h"
#include "iter/string.h"
#include "syscalls.h"
#include "join.h"
#include "fat16.h"
#include "format.h"

void _start(void* fdt) {
    uart_printf("initd started\n");

    alloc_t page_alloc = (alloc_t) PAGE_ALLOC;
    free_buckets_alloc_t free_buckets = free_buckets_allocator_options(&page_alloc, PAGE_ALLOC_PAGE_SIZE);
    alloc_t allocator = create_free_buckets_allocator(&free_buckets);

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

    capability_t cap;
    for (str_t part = str_split(module_maps, S("\n"), STREMPTY); part.bytes != NULL; part = str_split(module_maps, S("\n"), part)) {
        str_t device = str_split(part, S(" "), STREMPTY);
        str_t module = str_split(part, S(" "), device);
        uart_printf("%S maps to %S\n", device, module);
        void* module_elf = NULL;
        size_t module_size = 0;

        void* node = NULL;
        while ((node = fdt_find(&tree, device, node))) {
            uart_printf("found thing\n");
            if (module_elf == NULL) {
                char buffer[module.len + 1];
                memcpy(buffer, module.bytes, module.len);
                buffer[module.len] = '\0';

                module_elf = read_file_full(&initrd, buffer, &module_size);
                if (module_elf == NULL)
                    break;
            }

            uart_printf("spawning\n");
            spawn_process(module_elf, module_size, NULL, 0, &cap);
        }

        dealloc_page(module_elf, (module_size + PAGE_SIZE - 1) / PAGE_SIZE);
    }

    while(1) {
        pid_t pid;
        int type;
        uint64_t data;
        uint64_t meta;
        recv(true, &cap, &pid, &type, &data, &meta);
        uart_printf("pid, type, data, meta = %x, %x, %x, %x", pid, type, data, meta);
    }
}

