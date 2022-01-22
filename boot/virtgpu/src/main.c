#include "syscalls.h"
#include "join.h"
#include "virtio.h"

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
    virtqueue_add_to_device(data, alloc_queue, mmio, 0);
    virtqueue_add_to_device(data, alloc_queue, mmio, 1);
    return false;
}

void _start(void* _args, size_t _arg_size, uint64_t cap_high, uint64_t cap_low) {
    capability_t superdriver = ((capability_t) cap_high) << 64 | (capability_t) cap_low;

    pid_t pid;
    int type;
    uint64_t data;
    uint64_t meta;
    uart_printf("spawned virtio gpu driver\n");
    recv(true, &superdriver, &pid, &type, &data, &meta);
    subscribe_to_interrupt(data, &superdriver);
    recv(true, &superdriver, &pid, &type, &data, &meta);

    volatile virtio_mmio_t* mmio = (void*) data;
    switch (virtio_init_mmio(&superdriver, (void*) mmio, 16, features_callback, setup_callback)) {
        case VIRTIO_MMIO_INIT_STATE_SUCCESS:
            uart_printf("gpu driver initialisation successful\n");
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
            uart_printf("device is not a gpu\n");
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

    mmio->queue_notify = 1;

    recv(true, &superdriver, NULL, &type, &data, NULL);
    if (type == 4)
        uart_printf("received interrupt %x\n", data);

    while (1);
}
