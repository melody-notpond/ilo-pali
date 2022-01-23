#include "join.h"
#include "syscalls.h"
#include "virtio.h"
#include "phalloc.h"
#include "virtblock.h"

#define VIRTIO_BLK_F_RO 5

uint32_t features_callback(const volatile void* _config, uint32_t features) {
    return features;
}

virtual_physical_pair_t alloc_queue(void* data, size_t size) {
    capability_t* superdriver = data;
    send(true, superdriver, MSG_TYPE_SIGNAL, 2, (size + PAGE_SIZE - 1) / PAGE_SIZE);
    uint64_t virtual;
    uint64_t physical;
    recv(true, superdriver, NULL, NULL, &virtual, NULL);
    recv(true, superdriver, NULL, NULL, &physical, NULL);
    return (virtual_physical_pair_t) {
        .virtual_ = (void*) virtual,
        .physical = physical,
    };
}

bool setup_callback(void* data, volatile virtio_mmio_t* mmio) {
    void** p = data;
    *(void**) p[0] = virtqueue_add_to_device(p[1], alloc_queue, mmio, 0);
    return false;
}

void _start(void* _args, size_t _arg_size, uint64_t cap_high, uint64_t cap_low) {
    capability_t superdriver = ((capability_t) cap_high) << 64 | (capability_t) cap_low;

    pid_t pid;
    int type;
    uint64_t data;
    uint64_t meta;
    uart_printf("spawned virtio block driver\n");

    // Get interrupt id
    recv(true, &superdriver, &pid, &type, &data, &meta);
    subscribe_to_interrupt(data, &superdriver);

    virtio_queue_t* requestq = NULL;
    void* input[] = { (void*) &requestq, (void*) &superdriver };

    // Get mmio address
    recv(true, &superdriver, &pid, &type, &data, &meta);
    volatile virtio_mmio_t* mmio = (void*) data;
    switch (virtio_init_mmio(input, (void*) mmio, 2, features_callback, setup_callback)) {
        case VIRTIO_MMIO_INIT_STATE_SUCCESS:
            uart_printf("block driver initialisation successful\n");
            break;
        case VIRTIO_MMIO_INIT_STATE_INVALID_MMIO:
            uart_printf("invalid mmio passed in\n");
            kill(getpid());
            break;
        case VIRTIO_MMIO_INIT_STATE_DEVICE_ID_0:
            uart_printf("no device detected\n");
            kill(getpid());
            break;
        case VIRTIO_MMIO_INIT_STATE_UNKNOWN_DEVICE:
            uart_printf("device is not a block device\n");
            kill(getpid());
            break;
        case VIRTIO_MMIO_INIT_STATE_UNSUPPORTED_FEATURES:
            uart_printf("unsupported features passed in\n");
            kill(getpid());
            break;
        case VIRTIO_MMIO_INIT_STATE_UNKNOWN_ERROR:
            uart_printf("an unknown error occurred\n");
            kill(getpid());
            break;
    }

    phalloc_t phalloc = physical_allocator_options(&superdriver, 0);
    alloc_t allocator = create_physical_allocator(&phalloc);

    uint8_t* sector = alloc(&allocator, 512);
    uint8_t* status = alloc(&allocator, 1);
    *status = 69;
    struct virtio_blk_req* header = alloc(&allocator, sizeof(struct virtio_blk_req));
    *header = (struct virtio_blk_req) {
        .type = VIRTIO_BLK_T_IN,
        .sector = 0,
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
        .flags = VIRTIO_DESCRIPTOR_FLAG_WRITE_ONLY | VIRTIO_DESCRIPTOR_FLAG_NEXT,
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

    recv(true, &superdriver, NULL, &type, NULL, NULL);
    virtqueue_pop_used(requestq);
    virtqueue_pop_used(requestq);
    virtqueue_pop_used(requestq);

    if (*status != 0) {
        uart_printf("failed to read from sector 0. quitting\n");
        kill(getpid());
    } else uart_printf("read from sector 0\n");

    *header = (struct virtio_blk_req) {
        .type = VIRTIO_BLK_T_OUT,
        .sector = 0,
    };
    sector[0] = 0x69;
    sector[1] = 0x42;
    *status = 69;

    *virtqueue_push_descriptor(requestq, &d3) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(status),
        .flags = VIRTIO_DESCRIPTOR_FLAG_WRITE_ONLY,
        .length = 1,
        .next = 0,
    };

    *virtqueue_push_descriptor(requestq, &d2) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(sector),
        .flags = VIRTIO_DESCRIPTOR_FLAG_NEXT,
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

    recv(true, &superdriver, NULL, &type, NULL, NULL);
    virtqueue_pop_used(requestq);
    virtqueue_pop_used(requestq);
    virtqueue_pop_used(requestq);

    if (*status != 0) {
        uart_printf("failed to write to sector 0. quitting\n");
        kill(getpid());
    } else uart_printf("wrote to sector 0\n");

    while(1);
}
