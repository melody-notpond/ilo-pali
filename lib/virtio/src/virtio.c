#include "virtio.h"

#define VIRTIO_MAGIC 0x74726976

#define MMIO_ACKNOWLEDGE    1
#define MMIO_DRIVER         2
#define MMIO_FAILED         128
#define MMIO_FEATURES_OK    8
#define MMIO_DRIVER_OK      4
#define MMIO_NEEDS_RESET    64

// virtio_init_mmio(void*, void*, uint32_t, fn(const volatile void*, uint32_t) -> uint32_t, fn(void*, volatile virtio_mmio_t*) -> bool) -> virtio_mmio_init_state_t
// Initialises a virtio device using the mmio mapped to it.
virtio_mmio_init_state_t virtio_init_mmio(
    void* data,
    void* m,
    uint32_t requested_device_id,
    uint32_t (*features_callback)(const volatile void* config, uint32_t features),
    bool (*setup_callback)(void* data, volatile virtio_mmio_t* mmio)
) {
    volatile virtio_mmio_t* mmio = m;
    if (mmio->magic_value != VIRTIO_MAGIC || mmio->version != 0x2)
        return VIRTIO_MMIO_INIT_STATE_INVALID_MMIO;
    if (mmio->device_id == 0)
        return VIRTIO_MMIO_INIT_STATE_DEVICE_ID_0;
    if (mmio->device_id != requested_device_id)
        return VIRTIO_MMIO_INIT_STATE_UNKNOWN_DEVICE;

    // Step 1: reset
    mmio->status = 0;

    // Step 2: acknowledge the device
    mmio->status |= MMIO_ACKNOWLEDGE;

    // Step 3: state the driver knows how to use the device
    mmio->status |= MMIO_DRIVER;

    // Step 4: read device feature bits and write a subset of them
    uint32_t features = mmio->device_features;
#ifdef DISABLE_VIRTIO_F_RING_EVENT_IDX
    features &= ~(1 << 29);
#endif /* DISABLE_VIRTIO_F_RING_EVENT_IDX */
    features = features_callback(mmio->config, features);
    mmio->driver_features = features;

    // Step 5: set FEATURES_OK status bit
    mmio->status |= MMIO_FEATURES_OK;

    // Step 6: re-read status and check if FEATURES_OK is still set
    if ((mmio->status & MMIO_FEATURES_OK) == 0)
        return VIRTIO_MMIO_INIT_STATE_UNSUPPORTED_FEATURES;

    // Step 7: device specific setup
    if (setup_callback(data, mmio))
        return VIRTIO_MMIO_INIT_STATE_UNKNOWN_ERROR;

    // Step 8: set DRIVER_OK and we are live!
    mmio->status |= MMIO_DRIVER_OK;
    return VIRTIO_MMIO_INIT_STATE_SUCCESS;
}

// virtqueue_add_to_device(void*, fn(void*, size_t) -> void*, volatile virtio_mmio_t* mmio, uint32_t) -> virtio_queue_t*
// Adds a virtqueue to a device.
virtio_queue_t* virtqueue_add_to_device(void* data, virtual_physical_pair_t (*alloc)(void*, size_t), volatile virtio_mmio_t* mmio, uint32_t queue_sel) {
    // Check queue size
    uint32_t queue_num_max = mmio->queue_num_max;

    // If the queue size is too small, give up
    if (queue_num_max < VIRTIO_RING_SIZE) {
        return (virtio_queue_t*) 0;
    }

    // Select queue
    mmio->queue_sel = queue_sel;

    // Set queue size
    mmio->queue_num = VIRTIO_RING_SIZE;

    // Check that queue is not in use
    if (mmio->queue_ready) {
        return (virtio_queue_t*) 0;
    }

    // Allocate queue
    virtual_physical_pair_t pair = alloc(data, sizeof(virtio_queue_t) + VIRTIO_RING_SIZE * sizeof(virtio_descriptor_t) + sizeof(virtio_available_t) + sizeof(virtio_used_t));
    virtio_queue_t* queue = pair.virtual_;
    void* ptr = (void*) queue;
    uint64_t physical = pair.physical;
    queue->num = 0;
    queue->last_seen_used = 0;
    queue->desc = (ptr += sizeof(virtio_queue_t));
    uint64_t desc = (physical += sizeof(virtio_queue_t));
    queue->available = (ptr += VIRTIO_RING_SIZE * sizeof(virtio_descriptor_t));
    uint64_t available = (physical += VIRTIO_RING_SIZE * sizeof(virtio_descriptor_t));
    queue->available->flags = 0;
    queue->available->idx = 0;
    queue->used = (ptr += sizeof(virtio_available_t));
    uint64_t used = (physical += sizeof(virtio_available_t));
    queue->used->flags = 0;
    queue->used->idx = 0;

    // Notify device of queue
    mmio->queue_num = VIRTIO_RING_SIZE;

    // Give device queue addresses
    mmio->queue_desc_low = (uint32_t) desc;
    mmio->queue_desc_high = ((uint32_t) (desc >> 32));
    mmio->queue_avail_low = (uint32_t) available;
    mmio->queue_avail_high = ((uint32_t) (available >> 32));
    mmio->queue_used_low = (uint32_t) used;
    mmio->queue_used_high = (uint32_t) (used >> 32);

    // State that queue is ready
    mmio->queue_ready = 1;
    return queue;
}

// virtqueue_push_descriptor(virtio_queue_t*, uint16_t*) -> volatile virtio_descriptor_t*
// Pushes a descriptor to a queue.
volatile virtio_descriptor_t* virtqueue_push_descriptor(virtio_queue_t* queue, uint16_t* desc_index) {
    *desc_index = queue->num;
    volatile virtio_descriptor_t* desc = queue->desc + queue->num;
    queue->num++;
    while (queue->num >= VIRTIO_RING_SIZE)
        queue->num -= VIRTIO_RING_SIZE;

    return desc;
}

// virtqueue_push_available(virtio_queue_t*, uint16_t) -> void
// Pushes an available descriptor to a queue.
void virtqueue_push_available(virtio_queue_t* queue, uint16_t desc) {
    queue->available->ring[queue->available->idx++ % VIRTIO_RING_SIZE] = desc;
}

// virtqueue_pop_used(virtio_queue_t*) -> volatile virtio_descriptor_t*
// Pops a used descriptor from a queue.
volatile virtio_descriptor_t* virtqueue_pop_used(virtio_queue_t* queue) {
    if (queue->last_seen_used == queue->used->idx)
        return (void*) 0;

    uint16_t id = queue->used->ring[queue->last_seen_used++ % VIRTIO_RING_SIZE].id;
    volatile virtio_descriptor_t* used = &queue->desc[id];

    return used;
}
