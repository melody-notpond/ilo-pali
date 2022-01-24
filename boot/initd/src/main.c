#include <stddef.h>

#include "fdt.h"
#include "iter/string.h"
#include "syscalls.h"
#include "join.h"
#include "fat16.h"
#include "format.h"

struct thread_args {
    capability_t cap;
    alloc_t alloc;
    fat16_fs_t* initrd;
};

void handle_driver(void* args, size_t _size, uint64_t _a, uint64_t _b) {
    capability_t capability = ((struct thread_args*) args)->cap;
    alloc_t allocator = ((struct thread_args*) args)->alloc;
    fat16_fs_t* initrd = ((struct thread_args*) args)->initrd;

    pid_t pid;
    int type;
    uint64_t data;
    uint64_t meta;
    while (!recv(true, &capability, &pid, &type, &data, &meta)) {
        if (type == MSG_TYPE_DATA) {
            char buffer[meta + 1];
            memcpy(buffer, (void*) data, meta);
            buffer[meta] = '\0';

            size_t size;
            void* elf = read_file_full(initrd, buffer, &size);
            if (elf == NULL) {
                dealloc_page((void*) data, 1);
                continue;
            }

            capability_t cap;
            spawn_process(elf, size, NULL, 0, &cap);
            send(true, &capability, MSG_TYPE_INT, cap & 0xffffffffffffffff, cap >> 64);
            dealloc_page((void*) data, 1);
        }
    }

    dealloc(&allocator, args);
    kill(getpid());
}

int x;

void _start(void* fdt) {
    asm volatile(
        ".option push\n"
        ".option norelax\n"
        "lla gp, __global_pointer$\n"
        ".option pop\n"
    );

    x = 2;

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
    bool spawned_fsd = false;
    for (str_t part = str_split(module_maps, S("\n"), STREMPTY); part.bytes != NULL; part = str_split(module_maps, S("\n"), part)) {
        str_t device = str_split(part, S(" "), STREMPTY);
        str_t module = str_split(part, S(" "), device);
        uart_printf("%S maps to %S\n", device, module);
        void* module_elf = NULL;
        size_t module_size = 0;

        if (!spawned_fsd && str_equals(device, S("fs"))) {
            spawned_fsd = true;
            uart_printf("spawning\n");
            if (module_elf == NULL) {
                char buffer[module.len + 1];
                memcpy(buffer, module.bytes, module.len);
                buffer[module.len] = '\0';

                module_elf = read_file_full(&initrd, buffer, &module_size);
                if (module_elf == NULL)
                    break;
            }
            spawn_process(module_elf, module_size, NULL, 0, &cap);
            struct thread_args* args = alloc(&allocator, sizeof(struct thread_args));
            args->cap = cap;
            args->alloc = allocator;
            args->initrd = &initrd;
            spawn_thread(handle_driver, args, sizeof(struct thread_args), NULL);
        } else {
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
                uint64_t* p = alloc(&allocator, 3 * sizeof(uint64_t));
                struct fdt_property reg = fdt_get_property(&tree, node, "reg");
                struct fdt_property interrupts = fdt_get_property(&tree, node, "interrupts");
                p[0] = be_to_le(64, reg.data);
                p[1] = be_to_le(64, reg.data + 8); // TODO: use #address-cells and #size-cells
                p[2] = be_to_le(interrupts.len * 8, interrupts.data);
                spawn_process(module_elf, module_size, p, 3 * sizeof(uint64_t), &cap);
                dealloc(&allocator, p);
                struct thread_args* args = alloc(&allocator, sizeof(struct thread_args));
                args->cap = cap;
                args->alloc = allocator;
                args->initrd = &initrd;
                spawn_thread(handle_driver, args, sizeof(struct thread_args), NULL);
            }

            dealloc_page(module_elf, (module_size + PAGE_SIZE - 1) / PAGE_SIZE);
        }
    }

    while(1);
}

