#include <stdatomic.h>
#include <stddef.h>

#include "fdt.h"
#include "iter/string.h"
#include "iter/vec.h"
#include "sync.h"
#include "syscalls.h"
#include "join.h"
#include "fat16.h"
#include "format.h"

struct thread_args {
    capability_t cap;
    pid_t pid;
    fat16_fs_t* initrd;
};

struct driver {
    capability_t cap;
    pid_t pid;
};

struct driver filesystem_handler = { 0 };
mutex_t* block_handlers;

void handle_driver(void* args, size_t _size, uint64_t _a, uint64_t _b) {
    capability_t capability = ((struct thread_args*) args)->cap;
    fat16_fs_t* initrd = ((struct thread_args*) args)->initrd;
    dealloc_page(args, 1);

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
                dealloc_page((void*) data, (size + PAGE_SIZE - 1) / PAGE_SIZE);
                continue;
            }

            capability_t cap;
            pid_t pid = spawn_process((void*) data, meta, elf, size, NULL, 0, &cap);
            send(true, &capability, MSG_TYPE_INT, pid, 0);
            transfer_capability(&cap, pid);
            send(true, &capability, MSG_TYPE_INT, cap & 0xffffffffffffffff, cap >> 64);
            dealloc_page((void*) data, 1);
        } else if (type == MSG_TYPE_SIGNAL) {
            // driver -> initd protocol:
            // - SIGNAL 0 - register driver
            //    - META 0 - register block driver
            //    - META 1 - register filesystem driver
            //
            // initd -> driver protocol:
            // - SIGNAL 0 - registration status
            //    - META 0 - registration success
            //    - META 1 - driver already registered
            // - SIGNAL 1 - capability transfer
            //    - META 0 - block driver capability
            //      INTEGER
            //         - capability low double word
            //         - capability high double word
            switch (data) {
                // Register driver
                case 0:
                    switch (meta) {
                        // Register block device driver
                        case 0: {
                            mutex_guard_t guard = mutex_lock(block_handlers);
                            vec_t* vec = guard.data;
                            vec_push(vec, &(struct driver) {
                                .cap = capability,
                                .pid = pid,
                            });

                            if (filesystem_handler.cap != 0) {
                                capability_t cap1;
                                capability_t cap2;
                                create_capability(&cap1, &cap2);
                                send(true, &filesystem_handler.cap, MSG_TYPE_SIGNAL, 1, 0);
                                transfer_capability(&cap1, filesystem_handler.pid);
                                send(true, &filesystem_handler.cap, MSG_TYPE_INT, cap1 & 0xffffffffffffffff, cap1 >> 64);

                                send(true, &capability, MSG_TYPE_SIGNAL, 0, 0);
                                transfer_capability(&cap2, pid);
                                send(true, &capability, MSG_TYPE_INT, cap2 & 0xffffffffffffffff, cap2 >> 64);
                            }

                            mutex_unlock(&guard);
                            break;
                        }

                        // Register file system driver if none is registered
                        case 1: {
                            if (filesystem_handler.cap == 0) {
                                filesystem_handler.cap = capability;
                                filesystem_handler.pid = pid;

                                send(true, &filesystem_handler.cap, MSG_TYPE_SIGNAL, 0, 0);
                                mutex_guard_t guard = mutex_lock(block_handlers);
                                vec_t* vec = guard.data;
                                size_t i = 0;
                                for (struct driver* driver = vec_get(vec, i); driver != NULL; driver = vec_get(vec, ++i)) {
                                    capability_t cap1;
                                    capability_t cap2;
                                    create_capability(&cap1, &cap2);
                                    send(true, &filesystem_handler.cap, MSG_TYPE_SIGNAL, 1, 0);
                                    transfer_capability(&cap1, filesystem_handler.pid);
                                    send(true, &filesystem_handler.cap, MSG_TYPE_INT, cap1 & 0xffffffffffffffff, cap1 >> 64);

                                    send(true, &driver->cap, MSG_TYPE_SIGNAL, 0, 0);
                                    transfer_capability(&cap2, driver->pid);
                                    send(true, &driver->cap, MSG_TYPE_INT, cap2 & 0xffffffffffffffff, cap2 >> 64);
                                }
                                mutex_unlock(&guard);
                            } else send(true, &capability, MSG_TYPE_SIGNAL, 0, 1);
                            break;
                        }

                        default:
                            uart_printf("unknown driver registration number %x\n", meta);
                    }

                    break;
                default:
                    uart_printf("unknown signal %x\n", data);
            }
        }
    }

    kill(getpid());
}

void _start(void* fdt) {
    asm volatile(
        ".option push\n"
        ".option norelax\n"
        "lla gp, __global_pointer$\n"
        ".option pop\n"
    );

    uart_printf("initd started\n");

    alloc_t page_alloc = (alloc_t) PAGE_ALLOC;
    free_buckets_alloc_t free_buckets = free_buckets_allocator_options(&page_alloc, PAGE_ALLOC_PAGE_SIZE);
    alloc_t allocator = create_free_buckets_allocator(&free_buckets);
    vec_t block_handlers_vec = vec(&allocator, struct driver);
    block_handlers = create_mutex(&allocator, &block_handlers_vec);

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
            pid_t pid = spawn_process((char*) module.bytes, module.len, module_elf, module_size, NULL, 0, &cap);
            struct thread_args* args = alloc_page(1, PERM_READ | PERM_WRITE);
            args->cap = cap;
            args->pid = pid;
            args->initrd = &initrd;
            while (1) {
                pid_t pid = spawn_thread(handle_driver, args, sizeof(struct thread_args), NULL);
                if (!transfer_capability(&cap, pid))
                    break;
            }
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
                struct fdt_property reg = fdt_get_property(&tree, node, "reg");
                struct fdt_property interrupts = fdt_get_property(&tree, node, "interrupts");
                uint64_t p[3];
                p[0] = be_to_le(64, reg.data);
                p[1] = be_to_le(64, reg.data + 8); // TODO: use #address-cells and #size-cells
                p[2] = be_to_le(interrupts.len * 8, interrupts.data);
                pid_t pid = spawn_process((char*) module.bytes, module.len, module_elf, module_size, p, sizeof(p), &cap);
                struct thread_args* args = alloc_page(1, PERM_READ | PERM_WRITE);
                args->cap = cap;
                args->pid = pid;
                args->initrd = &initrd;
                while (1) {
                    pid_t pid = spawn_thread(handle_driver, args, sizeof(struct thread_args), NULL);
                    if (!transfer_capability(&cap, pid))
                        break;
                }
            }

            dealloc_page(module_elf, (module_size + PAGE_SIZE - 1) / PAGE_SIZE);
        }
    }

    while(1);
}

