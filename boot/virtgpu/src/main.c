#include "syscalls.h"
#include "join.h"
#include "phalloc.h"
#include "virtgpu.h"
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
    void** p = data;
    *(void**) p[0] = virtqueue_add_to_device(p[2], alloc_queue, mmio, 0);
    *(void**) p[1] = virtqueue_add_to_device(p[2], alloc_queue, mmio, 1);
    return false;
}

void _start(void* _args, size_t _arg_size, uint64_t cap_high, uint64_t cap_low) {
    capability_t superdriver = ((capability_t) cap_high) << 64 | (capability_t) cap_low;

    pid_t pid;
    int type;
    uint64_t data;
    uint64_t meta;
    uart_printf("spawned virtio gpu driver\n");

    // Get interrupt id
    recv(true, &superdriver, &pid, &type, &data, &meta);
    subscribe_to_interrupt(data, &superdriver);

    virtio_queue_t* controlq;
    virtio_queue_t* cursorq;
    void* input[] = { (void*) &controlq, (void*) &cursorq, (void*) &superdriver };

    // Get mmio address
    recv(true, &superdriver, &pid, &type, &data, &meta);
    volatile virtio_mmio_t* mmio = (void*) data;
    switch (virtio_init_mmio(input, (void*) mmio, 16, features_callback, setup_callback)) {
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

    phalloc_t phalloc = physical_allocator_options(&superdriver, 0);
    alloc_t allocator = create_physical_allocator(&phalloc);

    // Get display info
    struct virtio_gpu_ctrl_hdr* header = alloc(&allocator, sizeof(struct virtio_gpu_ctrl_hdr));
    *header = (struct virtio_gpu_ctrl_hdr) {
        .type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO,
        .flags = 0,
        .fence_id = 0,
        .ctx_id = 0,
    };

    struct virtio_gpu_resp_display_info* info = alloc(&allocator, sizeof(struct virtio_gpu_resp_display_info));

    uint16_t d1, d2;
    *virtqueue_push_descriptor(controlq, &d2) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(info),
        .flags = VIRTIO_DESCRIPTOR_FLAG_WRITE_ONLY,
        .length = sizeof(struct virtio_gpu_resp_display_info),
        .next = 0,
    };
    *virtqueue_push_descriptor(controlq, &d1) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(header),
        .flags = VIRTIO_DESCRIPTOR_FLAG_NEXT,
        .length = sizeof(struct virtio_gpu_ctrl_hdr),
        .next = d2,
    };
    virtqueue_push_available(controlq, d1);
    mmio->queue_notify = 0;

    recv(true, &superdriver, NULL, &type, &data, NULL);
    virtqueue_pop_used(controlq);
    virtqueue_pop_used(controlq);
    if (type == 4) {
        if (info->hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO) {
            uart_printf("[gpu driver] failed to get display info. quitting");
            kill(getpid());
        } else uart_printf("[gpu driver] got display info\n");
    } else {
        uart_printf("oh no\n");
        kill(getpid());
    }

    // Create 2D resource
    struct virtio_gpu_resource_create_2d* create = alloc(&allocator, sizeof(struct virtio_gpu_resource_create_2d));
    *create = (struct virtio_gpu_resource_create_2d) {
        .hdr = {
            .type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
            .flags = 0,
            .fence_id = 0,
            .ctx_id = 0,
        },
        .resource_id = 1,
        .format = VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM,
        .width = info->pmodes[0].r.width,
        .height = info->pmodes[0].r.height,
    };

    *virtqueue_push_descriptor(controlq, &d2) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(header),
        .flags = VIRTIO_DESCRIPTOR_FLAG_WRITE_ONLY,
        .length = sizeof(struct virtio_gpu_ctrl_hdr),
        .next = 0,
    };
    *virtqueue_push_descriptor(controlq, &d1) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(create),
        .flags = VIRTIO_DESCRIPTOR_FLAG_NEXT,
        .length = sizeof(struct virtio_gpu_resource_create_2d),
        .next = d2,
    };
    virtqueue_push_available(controlq, d1);
    mmio->queue_notify = 0;

    recv(true, &superdriver, NULL, &type, &data, NULL);
    virtqueue_pop_used(controlq);
    virtqueue_pop_used(controlq);
    if (type == 4) {
        if (header->type != VIRTIO_GPU_RESP_OK_NODATA) {
            uart_printf("[gpu driver] failed to create resource. quitting");
            kill(getpid());
        } else uart_printf("[gpu driver] created resource\n");
    } else {
        uart_printf("oh no\n");
        kill(getpid());
    }

    // Attach frame buffer
    size_t framebuffer_size = sizeof(uint32_t) * info->pmodes[0].r.width * info->pmodes[0].r.height;
    uint32_t* framebuffer = alloc(&allocator, framebuffer_size);
    struct virtio_gpu_resource_attach_backing* backing = alloc(&allocator, sizeof(struct virtio_gpu_resource_attach_backing) + sizeof(struct virtio_gpu_mem_entry));
    *backing = (struct virtio_gpu_resource_attach_backing) {
        .hdr = {
            .type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
            .flags = 0,
            .fence_id = 0,
            .ctx_id = 0,
        },
        .resource_id = 1,
        .nr_entries = 1,
    };
    backing->entries[0] = (struct virtio_gpu_mem_entry) {
        .addr = phalloc_get_physical(framebuffer),
        .length = framebuffer_size,
    };
    *virtqueue_push_descriptor(controlq, &d2) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(header),
        .flags = VIRTIO_DESCRIPTOR_FLAG_WRITE_ONLY,
        .length = sizeof(struct virtio_gpu_ctrl_hdr),
        .next = 0,
    };
    *virtqueue_push_descriptor(controlq, &d1) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(backing),
        .flags = VIRTIO_DESCRIPTOR_FLAG_NEXT,
        .length = sizeof(struct virtio_gpu_resource_attach_backing) + sizeof(struct virtio_gpu_mem_entry),
        .next = d2,
    };
    virtqueue_push_available(controlq, d1);
    mmio->queue_notify = 0;

    recv(true, &superdriver, NULL, &type, &data, NULL);
    virtqueue_pop_used(controlq);
    virtqueue_pop_used(controlq);
    if (type == 4) {
        if (header->type != VIRTIO_GPU_RESP_OK_NODATA) {
            uart_printf("[gpu driver] failed to attach framebuffer. quitting");
            kill(getpid());
        } else uart_printf("[gpu driver] attached framebuffer\n");
    } else {
        uart_printf("oh no\n");
        kill(getpid());
    }
    dealloc(&allocator, backing);

    // Set scanout
    struct virtio_gpu_set_scanout* scanout = alloc(&allocator, sizeof(struct virtio_gpu_set_scanout));
    *scanout = (struct virtio_gpu_set_scanout) {
        .hdr = {
            .type = VIRTIO_GPU_CMD_SET_SCANOUT,
            .flags = 0,
            .fence_id = 0,
            .ctx_id = 0,
        },
        .r = info->pmodes[0].r,
        .scanout_id = 0,
        .resource_id = 1,
    };
    *virtqueue_push_descriptor(controlq, &d2) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(header),
        .flags = VIRTIO_DESCRIPTOR_FLAG_WRITE_ONLY,
        .length = sizeof(struct virtio_gpu_ctrl_hdr),
        .next = 0,
    };
    *virtqueue_push_descriptor(controlq, &d1) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(scanout),
        .flags = VIRTIO_DESCRIPTOR_FLAG_NEXT,
        .length = sizeof(struct virtio_gpu_set_scanout),
        .next = d2,
    };

    virtqueue_push_available(controlq, d1);
    mmio->queue_notify = 0;

    recv(true, &superdriver, NULL, &type, &data, NULL);
    virtqueue_pop_used(controlq);
    virtqueue_pop_used(controlq);
    if (type == 4) {
        if (header->type != VIRTIO_GPU_RESP_OK_NODATA) {
            uart_printf("[gpu driver] failed to set scanout. quitting");
            kill(getpid());
        } else uart_printf("[gpu driver] set scanout. display is initialised!\n");
    } else {
        uart_printf("oh no\n");
        kill(getpid());
    }
    dealloc(&allocator, scanout);

    for (size_t i = 0; i < framebuffer_size / sizeof(uint32_t); i++)
        framebuffer[i] = 0xFFFF00FF;

    // Transfer resource to host
    struct virtio_gpu_transfer_to_host_2d* to_host = alloc(&allocator, sizeof(struct virtio_gpu_transfer_to_host_2d));
    *to_host = (struct virtio_gpu_transfer_to_host_2d) {
        .hdr = {
            .type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
            .flags = 0,
            .fence_id = 0,
            .ctx_id = 0,
        },
        .r = info->pmodes[0].r,
        .offset = 0,
        .resource_id = 1,
    };
    *virtqueue_push_descriptor(controlq, &d2) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(header),
        .flags = VIRTIO_DESCRIPTOR_FLAG_WRITE_ONLY,
        .length = sizeof(struct virtio_gpu_ctrl_hdr),
        .next = 0,
    };
    *virtqueue_push_descriptor(controlq, &d1) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(to_host),
        .flags = VIRTIO_DESCRIPTOR_FLAG_NEXT,
        .length = sizeof(struct virtio_gpu_transfer_to_host_2d),
        .next = d2,
    };

    virtqueue_push_available(controlq, d1);
    mmio->queue_notify = 0;

    recv(true, &superdriver, NULL, &type, &data, NULL);
    virtqueue_pop_used(controlq);
    virtqueue_pop_used(controlq);
    dealloc(&allocator, to_host);

    struct virtio_gpu_resource_flush* flush = alloc(&allocator, sizeof(struct virtio_gpu_resource_flush));
    *flush = (struct virtio_gpu_resource_flush) {
        .hdr = {
            .type = VIRTIO_GPU_CMD_RESOURCE_FLUSH,
            .flags = 0,
            .fence_id = 0,
            .ctx_id = 0,
        },
        .r = info->pmodes[0].r,
        .resource_id = 1,
    };
    *virtqueue_push_descriptor(controlq, &d2) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(header),
        .flags = VIRTIO_DESCRIPTOR_FLAG_WRITE_ONLY,
        .length = sizeof(struct virtio_gpu_ctrl_hdr),
        .next = 0,
    };
    *virtqueue_push_descriptor(controlq, &d1) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(flush),
        .flags = VIRTIO_DESCRIPTOR_FLAG_NEXT,
        .length = sizeof(struct virtio_gpu_transfer_to_host_2d),
        .next = d2,
    };

    virtqueue_push_available(controlq, d1);
    mmio->queue_notify = 0;

    recv(true, &superdriver, NULL, &type, &data, NULL);
    virtqueue_pop_used(controlq);
    virtqueue_pop_used(controlq);
    dealloc(&allocator, to_host);

    while (1);
}
