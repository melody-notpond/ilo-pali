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

void perform_block_operation(volatile virtio_mmio_t* mmio, mutex_guard_t* allocator_guard, mutex_t* requestq_mutex, bool read, uint64_t sector_index, void* sector, size_t sector_count) {
    alloc_t* allocator = allocator_guard->data;
    if (sector == NULL)
        sector = alloc(allocator, 512);
    uint8_t* status = alloc(allocator, 1);
    *status = 69;
    struct virtio_blk_req* header = alloc(allocator, sizeof(struct virtio_blk_req));
    *header = (struct virtio_blk_req) {
        .type = read ? VIRTIO_BLK_T_IN : VIRTIO_BLK_T_OUT,
        .sector = sector_index,
    };

    uint16_t d1, d2, d3;
    mutex_guard_t requestq_guard = mutex_lock(requestq_mutex);
    virtio_queue_t* requestq = requestq_guard.data;
    *virtqueue_push_descriptor(requestq, &d3) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(status),
        .flags = VIRTIO_DESCRIPTOR_FLAG_WRITE_ONLY,
        .length = 1,
        .next = 0,
    };

    *virtqueue_push_descriptor(requestq, &d2) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(sector),
        .flags = (read ? VIRTIO_DESCRIPTOR_FLAG_WRITE_ONLY : 0) | VIRTIO_DESCRIPTOR_FLAG_NEXT,
        .length = 512 * sector_count,
        .next = d3,
    };

    *virtqueue_push_descriptor(requestq, &d1) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(header),
        .flags = VIRTIO_DESCRIPTOR_FLAG_NEXT,
        .length = sizeof(struct virtio_blk_req),
        .next = d2,
    };

    virtqueue_push_available(requestq, d1);
    mutex_unlock(&requestq_guard);
    mutex_unlock(allocator_guard);
    mmio->queue_notify = 0;
}

struct handle_interrupts_args {
    uint64_t interrupt_id;
    mutex_t* alloc_mutex;
    mutex_t* requestq_mutex;
};

void handle_interrupts(void* args, size_t args_size, uint64_t cap_high, uint64_t cap_low) {
    capability_t interrupts = ((capability_t) cap_high) << 64 | (capability_t) cap_low;
    struct handle_interrupts_args* args_struct = args;

    subscribe_to_interrupt(args_struct->interrupt_id, &interrupts);

    int type;
    uint64_t data;
    while (!recv(true, &interrupts, NULL, &type, &data, NULL)) {
        if (type != MSG_TYPE_INTERRUPT)
            continue;
        mutex_guard_t requestq_guard = mutex_lock(args_struct->requestq_mutex);
        virtio_queue_t* requestq = requestq_guard.data;
        mutex_guard_t allocator_guard = mutex_lock(args_struct->alloc_mutex);
        alloc_t* alloc = allocator_guard.data;
        volatile virtio_descriptor_t* desc = virtqueue_pop_used(requestq);
        dealloc(alloc, phalloc_get_virtual(alloc->data, (uint64_t) desc->addr));
        desc = virtqueue_get_descriptor(requestq, desc->next);
        desc = virtqueue_get_descriptor(requestq, desc->next);
        uint8_t* status = phalloc_get_virtual(alloc->data, (uint64_t) desc->addr);
        if (*status != 0)
            uart_printf("[block driver] block operation failed\n");
        dealloc(alloc, status);
        mutex_unlock(&allocator_guard);
        mutex_unlock(&requestq_guard);
    }

    kill(getpid());
}

struct handle_process_args {
    mutex_t* alloc_mutex;
    mutex_t* requestq_mutex;
};

void handle_process(void* args, size_t _arg_size, uint64_t cap_high, uint64_t cap_low) {
    struct handle_process_args* args_struct = args;

    capability_t process = (capability_t) cap_high << 64 | (capability_t) cap_low;
    pid_t pid;
    int type;
    uint64_t data;
    uint64_t meta;

    while (!recv(true, &process, &pid, &type, &data, &meta)) {
    }
}

typedef enum {
    NONE,
} block_driver_state_t;

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

    struct {
        alloc_t alloc;
        phalloc_t phalloc;
    } A;
    A.phalloc = physical_allocator_options(&initd, 0);
    A.alloc = create_physical_allocator(&A.phalloc);
    mutex_t* alloc_mutex = create_mutex(&A.alloc, &A);
    mutex_t* requestq_mutex = create_mutex(&A.alloc, requestq);
    struct handle_interrupts_args args_struct = {
        .interrupt_id = interrupt_id,
        .alloc_mutex = alloc_mutex,
        .requestq_mutex = requestq_mutex,
    };
    capability_t capability;
    spawn_thread(handle_interrupts, &args_struct, sizeof(struct handle_interrupts_args), &capability);

    struct handle_process_args args = {
        .alloc_mutex = alloc_mutex,
        .requestq_mutex = requestq_mutex,
    };

    // Driver is live, send registration request to initd and kill superdriver
    send(true, &initd, MSG_TYPE_SIGNAL, 0, 0);
    send(true, &superdriver, MSG_TYPE_SIGNAL, 0, 0);

    pid_t fsd = 0;
    block_driver_state_t state = NONE;

    while(!recv(true, &initd, &pid, &type, &data, &meta)) {
        if (fsd == 0)
            fsd = pid;
        else if (fsd != pid)
            continue;
    }

    kill(getpid());
}
