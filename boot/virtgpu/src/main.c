#include "syscalls.h"
#include "join.h"
#include "phalloc.h"
#include "virtgpu.h"
#include "virtio.h"

uint32_t features_callback(const volatile void* _config, uint32_t features) {
    return features;
}

virtual_physical_pair_t alloc_queue(void* data, size_t size) {
    capability_t* initd = data;
    return alloc_pages_physical(NULL, (size + PAGE_SIZE - 1) / PAGE_SIZE, PERM_READ | PERM_WRITE, initd);
}

bool setup_callback(void* data, volatile virtio_mmio_t* mmio) {
    void** p = data;
    *(void**) p[0] = virtqueue_add_to_device(p[2], alloc_queue, mmio, 0);
    *(void**) p[1] = virtqueue_add_to_device(p[2], alloc_queue, mmio, 1);
    return false;
}

void perform_gpu_action(volatile virtio_mmio_t* mmio, virtio_queue_t* controlq, alloc_t* allocator, void* request, size_t request_size, size_t response_size) {
    void* phalloced_request = alloc(allocator, request_size);
    memcpy(phalloced_request, request, request_size);

    uint16_t d1, d2;
    *virtqueue_push_descriptor(controlq, &d2) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(alloc(allocator, response_size)),
        .flags = VIRTIO_DESCRIPTOR_FLAG_WRITE_ONLY,
        .length = response_size,
        .next = 0,
    };
    *virtqueue_push_descriptor(controlq, &d1) = (virtio_descriptor_t) {
        .addr = (void*) phalloc_get_physical(phalloced_request),
        .flags = VIRTIO_DESCRIPTOR_FLAG_NEXT,
        .length = request_size,
        .next = d2,
    };
    virtqueue_push_available(controlq, d1);
    mmio->queue_notify = 0;
}

void send_and_flush_framebuffer(volatile virtio_mmio_t* mmio, alloc_t* allocator, phalloc_t* phalloc, virtio_queue_t* controlq, capability_t cap, struct virtio_gpu_rect r, uint32_t resource_id) {
    // Transfer resource to host
    struct virtio_gpu_transfer_to_host_2d to_host = {
        .hdr = {
            .type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
            .flags = 0,
            .fence_id = 0,
            .ctx_id = 0,
        },
        .r = r,
        .offset = 0,
        .resource_id = resource_id,
    };

    int type;
    uint64_t data;

    perform_gpu_action(mmio, controlq, allocator, &to_host, sizeof(to_host), sizeof(struct virtio_gpu_ctrl_hdr));
    recv(true, cap, NULL, &type, &data, NULL);
    volatile virtio_descriptor_t* d = virtqueue_pop_used(controlq);
    dealloc(allocator, phalloc_get_virtual(phalloc, (uint64_t) d->addr));
    d = virtqueue_get_descriptor(controlq, d->next);
    dealloc(allocator, phalloc_get_virtual(phalloc, (uint64_t) d->addr));

    struct virtio_gpu_resource_flush flush = {
        .hdr = {
            .type = VIRTIO_GPU_CMD_RESOURCE_FLUSH,
            .flags = 0,
            .fence_id = 0,
            .ctx_id = 0,
        },
        .r = r,
        .resource_id = resource_id,
    };
    perform_gpu_action(mmio, controlq, allocator, &flush, sizeof(flush), sizeof(struct virtio_gpu_ctrl_hdr));
    recv(true, cap, NULL, &type, &data, NULL);
    d = virtqueue_pop_used(controlq);
    dealloc(allocator, phalloc_get_virtual(phalloc, (uint64_t) d->addr));
    d = virtqueue_get_descriptor(controlq, d->next);
    dealloc(allocator, phalloc_get_virtual(phalloc, (uint64_t) d->addr));
}

void _start(void* _args, size_t _arg_size) {
    capability_t superdriver = 0;

    pid_t pid;
    int type;
    uint64_t data;
    uint64_t meta;
    uart_printf("spawned virtio gpu driver\n");

    // Get initd capability
    recv(true, superdriver, &pid, &type, &data, &meta);
    capability_t initd = data;

    // Get interrupt id
    recv(true, superdriver, &pid, &type, &data, &meta);
    subscribe_to_interrupt(data, &superdriver);

    virtio_queue_t* controlq;
    virtio_queue_t* cursorq;
    void* input[] = { (void*) &controlq, (void*) &cursorq, (void*) &initd };

    // Get mmio address
    recv(true, superdriver, &pid, &type, &data, &meta);
    volatile virtio_mmio_t* mmio = (void*) data;
    switch (virtio_init_mmio(input, (void*) mmio, 16, features_callback, setup_callback)) {
        case VIRTIO_MMIO_INIT_STATE_SUCCESS:
            uart_printf("gpu driver initialisation successful\n");
            break;
        case VIRTIO_MMIO_INIT_STATE_INVALID_MMIO:
            uart_printf("invalid mmio passed in\n");
            exit(1);
            break;
        case VIRTIO_MMIO_INIT_STATE_DEVICE_ID_0:
            uart_printf("no device detected\n");
            exit(1);
            break;
        case VIRTIO_MMIO_INIT_STATE_UNKNOWN_DEVICE:
            uart_printf("device is not a gpu\n");
            exit(1);
            break;
        case VIRTIO_MMIO_INIT_STATE_UNSUPPORTED_FEATURES:
            uart_printf("unsupported features passed in\n");
            exit(1);
            break;
        case VIRTIO_MMIO_INIT_STATE_UNKNOWN_ERROR:
            uart_printf("an unknown error occurred\n");
            exit(1);
            break;
    }

    phalloc_t phalloc = physical_allocator_options(&initd, 0);
    alloc_t allocator = create_physical_allocator(&phalloc);

    // Get display info
    struct virtio_gpu_ctrl_hdr header = {
        .type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO,
        .flags = 0,
        .fence_id = 0,
        .ctx_id = 0,
    };

    perform_gpu_action(mmio, controlq, &allocator, &header, sizeof(header), sizeof(struct virtio_gpu_resp_display_info));
    recv(true, superdriver, NULL, &type, &data, NULL);
    volatile virtio_descriptor_t* d = virtqueue_pop_used(controlq);
    struct virtio_gpu_resp_display_info* info = NULL;
    if (type == 4) {
        dealloc(&allocator, phalloc_get_virtual(&phalloc, (uint64_t) d->addr));
        d = virtqueue_get_descriptor(controlq, d->next);
        info = phalloc_get_virtual(&phalloc, (uint64_t) d->addr);
        if (info->hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO) {
            uart_printf("[gpu driver] failed to get display info. quitting");
            exit(1);
        } else uart_printf("[gpu driver] got display info; size is %ix%i\n", info->pmodes[0].r.width, info->pmodes[0].r.height);
    } else {
        uart_printf("oh no\n");
        exit(1);
    }

    // Create 2D resource
    struct virtio_gpu_resource_create_2d create = {
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
    perform_gpu_action(mmio, controlq, &allocator, &create, sizeof(create), sizeof(struct virtio_gpu_ctrl_hdr));

    recv(true, superdriver, NULL, &type, &data, NULL);
    d = virtqueue_pop_used(controlq);
    if (type == 4) {
        dealloc(&allocator, phalloc_get_virtual(&phalloc, (uint64_t) d->addr));
        d = virtqueue_get_descriptor(controlq, d->next);
        struct virtio_gpu_ctrl_hdr* header = phalloc_get_virtual(&phalloc, (uint64_t) d->addr);
        if (header->type != VIRTIO_GPU_RESP_OK_NODATA) {
            uart_printf("[gpu driver] failed to create resource. quitting");
            exit(1);
        } else uart_printf("[gpu driver] created resource\n");
        dealloc(&allocator, header);
    } else {
        uart_printf("oh no\n");
        exit(1);
    }

    // Attach frame buffer
    size_t framebuffer_size = sizeof(uint32_t) * info->pmodes[0].r.width * info->pmodes[0].r.height;
    uint32_t* framebuffer = alloc(&allocator, framebuffer_size);
    size_t backing_size = sizeof(struct virtio_gpu_resource_attach_backing) + sizeof(struct virtio_gpu_mem_entry);
    struct virtio_gpu_resource_attach_backing* backing = alloc(&allocator, backing_size);
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
    perform_gpu_action(mmio, controlq, &allocator, backing, backing_size, sizeof(struct virtio_gpu_ctrl_hdr));

    recv(true, superdriver, NULL, &type, &data, NULL);
    d = virtqueue_pop_used(controlq);
    if (type == 4) {
        dealloc(&allocator, phalloc_get_virtual(&phalloc, (uint64_t) d->addr));
        d = virtqueue_get_descriptor(controlq, d->next);
        struct virtio_gpu_ctrl_hdr* header = phalloc_get_virtual(&phalloc, (uint64_t) d->addr);
        if (header->type != VIRTIO_GPU_RESP_OK_NODATA) {
            uart_printf("[gpu driver] failed to attach framebuffer. quitting");
            exit(1);
        } else uart_printf("[gpu driver] attached framebuffer\n");
        dealloc(&allocator, header);
    } else {
        uart_printf("oh no\n");
        exit(1);
    }
    dealloc(&allocator, backing);

    // Set scanout
    struct virtio_gpu_set_scanout scanout = {
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
    perform_gpu_action(mmio, controlq, &allocator, &scanout, sizeof(scanout), sizeof(struct virtio_gpu_ctrl_hdr));

    recv(true, superdriver, NULL, &type, &data, NULL);
    d = virtqueue_pop_used(controlq);
    if (type == 4) {
        dealloc(&allocator, phalloc_get_virtual(&phalloc, (uint64_t) d->addr));
        d = virtqueue_get_descriptor(controlq, d->next);
        struct virtio_gpu_ctrl_hdr* header = phalloc_get_virtual(&phalloc, (uint64_t) d->addr);
        if (header->type != VIRTIO_GPU_RESP_OK_NODATA) {
            uart_printf("[gpu driver] failed to set scanout. quitting");
            exit(1);
        } else uart_printf("[gpu driver] set scanout. screen is initialised!\n");
        dealloc(&allocator, header);
    } else {
        uart_printf("oh no\n");
        exit(1);
    }

    for (size_t i = 0; i < framebuffer_size / sizeof(uint32_t); i++) {
        framebuffer[i] = 0xFFFF00FF;
    }

    send_and_flush_framebuffer(mmio, &allocator, &phalloc, controlq, superdriver, info->pmodes[0].r, 1);

    while (1);
}
