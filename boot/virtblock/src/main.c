#include "join.h"
#include "syscalls.h"
#include "virtio.h"
#include "phalloc.h"
#include "virtblock.h"
#include "sync.h"

#define VIRTIO_BLK_F_RO 5

uint32_t features_callback(const volatile void* _config, uint32_t features) {
    return features;
}

virtual_physical_pair_t alloc_queue(void* data, size_t size) {
    capability_t* initd = data;
    return alloc_pages_physical(NULL, (size + PAGE_SIZE - 1) / PAGE_SIZE, PERM_READ | PERM_WRITE, initd);
}

bool setup_callback(void* data, volatile virtio_mmio_t* mmio) {
    void** p = data;
    *(void**) p[0] = virtqueue_add_to_device(p[1], alloc_queue, mmio, 0);
    return false;
}

void perform_block_operation(volatile virtio_mmio_t* mmio, alloc_t* allocator, virtio_queue_t* requestq, bool read, uint64_t sector_index, void* sector) {
    if (sector == NULL)
        sector = alloc(allocator, 512);
    else {
        void* sector_new = alloc(allocator, 512);
        memcpy(sector_new, sector, 512);
        sector = sector_new;
    }

    uint8_t* status = alloc(allocator, 1);
    *status = 69;
    struct virtio_blk_req* header = alloc(allocator, sizeof(struct virtio_blk_req));
    *header = (struct virtio_blk_req) {
        .type = read ? VIRTIO_BLK_T_IN : VIRTIO_BLK_T_OUT,
        .sector = sector_index,
    };

    uint16_t d1, d2, d3;
    *virtqueue_push_descriptor(requestq, &d3) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(status),
        .flags = VIRTIO_DESCRIPTOR_FLAG_WRITE_ONLY,
        .length = 1,
        .next = 0,
    };

    *virtqueue_push_descriptor(requestq, &d2) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(sector),
        .flags = (read ? VIRTIO_DESCRIPTOR_FLAG_WRITE_ONLY : 0) | VIRTIO_DESCRIPTOR_FLAG_NEXT,
        .length = 512,
        .next = d3,
    };

    *virtqueue_push_descriptor(requestq, &d1) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(header),
        .flags = VIRTIO_DESCRIPTOR_FLAG_NEXT,
        .length = sizeof(struct virtio_blk_req),
        .next = d2,
    };

    virtqueue_push_available(requestq, d1);
    mmio->queue_notify = 0;
}

struct handle_interrupts_args {
    uint64_t interrupt_id;
    mutex_t* alloc_mutex;
    mutex_t* requestq_mutex;
};

struct handle_process_args {
    mutex_t* alloc_mutex;
    mutex_t* requestq_mutex;
};

void _start(void* _args, size_t _arg_size, uint64_t cap_high, uint64_t cap_low) {
    capability_t superdriver = ((capability_t) cap_high) << 64 | (capability_t) cap_low;

    pid_t pid;
    int type;
    uint64_t data;
    uint64_t meta;
    uart_printf("[block driver] spawned virtio block driver\n");

    // Get initd capability
    recv(true, &superdriver, &pid, &type, &data, &meta);
    capability_t initd = (capability_t) meta << 64 | (capability_t) data;

    // Get interrupt id
    uint64_t interrupt_id;
    recv(true, &superdriver, &pid, &type, &interrupt_id, &meta);

    virtio_queue_t* requestq;
    void* input[] = { (void*) &requestq, (void*) &initd };

    // Get mmio address
    recv(true, &superdriver, &pid, &type, &data, &meta);
    volatile virtio_mmio_t* mmio = (void*) data;
    switch (virtio_init_mmio(input, (void*) mmio, 2, features_callback, setup_callback)) {
        case VIRTIO_MMIO_INIT_STATE_SUCCESS:
            uart_printf("[block driver] block driver initialisation successful\n");
            break;
        case VIRTIO_MMIO_INIT_STATE_INVALID_MMIO:
            uart_printf("[block driver] invalid mmio passed in\n");
            send(true, &superdriver, MSG_TYPE_SIGNAL, 0, 0);
            kill(getpid());
            break;
        case VIRTIO_MMIO_INIT_STATE_DEVICE_ID_0:
            uart_printf("[block driver] no device detected\n");
            send(true, &superdriver, MSG_TYPE_SIGNAL, 0, 0);
            kill(getpid());
            break;
        case VIRTIO_MMIO_INIT_STATE_UNKNOWN_DEVICE:
            uart_printf("[block driver] device is not a block device\n");
            send(true, &superdriver, MSG_TYPE_SIGNAL, 0, 0);
            kill(getpid());
            break;
        case VIRTIO_MMIO_INIT_STATE_UNSUPPORTED_FEATURES:
            uart_printf("[block driver] unsupported features passed in\n");
            send(true, &superdriver, MSG_TYPE_SIGNAL, 0, 0);
            kill(getpid());
            break;
        case VIRTIO_MMIO_INIT_STATE_UNKNOWN_ERROR:
            uart_printf("[block driver] an unknown error occurred\n");
            send(true, &superdriver, MSG_TYPE_SIGNAL, 0, 0);
            kill(getpid());
            break;
    }

    // Driver is live, send registration request to initd and kill superdriver
    send(true, &initd, MSG_TYPE_SIGNAL, 0, 0);
    send(true, &superdriver, MSG_TYPE_SIGNAL, 0, 0);

    // Get fsd capability
    recv(true, &initd, &pid, &type, &data, &meta);
    recv(true, &initd, &pid, &type, &data, &meta);
    capability_t fsd = (capability_t) meta << 64 | (capability_t) data;

    phalloc_t phalloc = physical_allocator_options(&initd, 0);
    alloc_t allocator = create_physical_allocator(&phalloc);

    pid_t fsdp = 0;
    bool state_write = false;
    uint64_t sector_index = 0;

    subscribe_to_interrupt(interrupt_id, &fsd);
    while(!recv(true, &fsd, &pid, &type, &data, &meta)) {
        if (type == MSG_TYPE_INTERRUPT) {
            volatile virtio_descriptor_t* desc = virtqueue_pop_used(requestq);
            struct virtio_blk_req* header = phalloc_get_virtual(&phalloc, (uint64_t) desc->addr);
            bool read = header->type == VIRTIO_BLK_T_IN;
            dealloc(&allocator, header);
            desc = virtqueue_get_descriptor(requestq, desc->next);
            void* data = phalloc_get_virtual(&phalloc, (uint64_t) desc->addr);

            if (!read)
                dealloc(&allocator, data);
            desc = virtqueue_get_descriptor(requestq, desc->next);
            uint8_t* status = phalloc_get_virtual(&phalloc, (uint64_t) desc->addr);
            if (*status != 0) {
                uart_printf("[block driver] block operation failed\n");
                send(true, &fsd, MSG_TYPE_SIGNAL, 0, 2);
            } else if (read) {
                send(true, &fsd, MSG_TYPE_SIGNAL, 0, 0);
                send(true, &fsd, MSG_TYPE_DATA, (uint64_t) data, 512);
                dealloc(&allocator, data);
            } else {
                send(true, &fsd, MSG_TYPE_SIGNAL, 0, 1);
            }
            dealloc(&allocator, status);
            dealloc(&allocator, data);
        } else {
            if (fsdp == 0)
                fsdp = pid;
            else if (fsdp != pid)
                continue;

            if (!state_write) {
                if (type == MSG_TYPE_SIGNAL) {
                    switch (data) {
                        // READ
                        case 0:
                            perform_block_operation(mmio, &allocator, requestq, true, meta, NULL);
                            break;

                        // WRITE DATA
                        case 1:
                            sector_index = meta;
                            state_write = true;
                            break;

                        default:
                            uart_printf("[block driver] unknown signal type %x\n", data);
                    }
                } else if (type == MSG_TYPE_POINTER || type == MSG_TYPE_DATA) {
                    dealloc_page((void*) data, (meta + PAGE_SIZE - 1) / PAGE_SIZE);
                }
            } else {
                if (type == MSG_TYPE_DATA && meta == 512) {
                    perform_block_operation(mmio, &allocator, requestq, false, sector_index, (void*) data);
                    dealloc_page((void*) data, 1);
                } else if (type == MSG_TYPE_DATA || type == MSG_TYPE_POINTER)
                    dealloc_page((void*) data, (meta + PAGE_SIZE - 1) / PAGE_SIZE);
                state_write = false;
            }
        }
    }

    kill(getpid());
}
