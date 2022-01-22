#ifndef VIRTIO_H
#define VIRTIO_H

#include "core/prelude.h"
#include "alloc.h"

#define DISABLE_VIRTIO_F_RING_EVENT_IDX

typedef enum {
    VIRTIO_MMIO_INIT_STATE_SUCCESS,
    VIRTIO_MMIO_INIT_STATE_INVALID_MMIO,
    VIRTIO_MMIO_INIT_STATE_DEVICE_ID_0,
    VIRTIO_MMIO_INIT_STATE_UNKNOWN_DEVICE,
    VIRTIO_MMIO_INIT_STATE_UNSUPPORTED_FEATURES,
    VIRTIO_MMIO_INIT_STATE_UNKNOWN_ERROR,
} virtio_mmio_init_state_t;

typedef struct __attribute__((__packed__, aligned(4))) {
    volatile const uint32_t magic_value;
	volatile const uint32_t version;
	volatile const uint32_t device_id;
	volatile const uint32_t vendor_id;

	volatile const uint32_t device_features;
	volatile uint32_t device_features_sel;
	volatile const uint8_t rsv1[8];

	volatile uint32_t driver_features;
	volatile uint32_t driver_features_sel;
	volatile const uint8_t rsv2[8];

	volatile uint32_t queue_sel;
	volatile const uint32_t queue_num_max;
	volatile uint32_t queue_num;
	volatile const uint8_t rsv3[4];

	volatile const uint8_t rsv4[4];
	volatile uint32_t queue_ready;
	volatile const uint8_t rsv5[8];

	volatile uint32_t queue_notify;
	volatile const uint8_t rsv6[12];

	volatile const uint32_t interrupt_status;
	volatile uint32_t interrupt_ack;
	volatile const uint8_t rsv7[8];

	volatile uint32_t status;
	volatile const uint8_t rsv8[12];

    volatile uint32_t queue_desc_low;
    volatile uint32_t queue_desc_high;
	volatile const uint8_t rsv9[8];

    volatile uint32_t queue_avail_low;
    volatile uint32_t queue_avail_high;
	volatile const uint8_t rsv10[8];

    volatile uint32_t queue_used_low;
    volatile uint32_t queue_used_high;
	volatile const uint8_t rsv11[8];

	volatile const uint8_t rsv12[0x4c];
    volatile const uint32_t config_gen;

	volatile char config[];
} virtio_mmio_t;

// virtio_init_mmio(void*, uint32_t, fn(const volatile void*, uint32_t) -> uint32_t, fn(volatile virtio_mmio_t*) -> bool) -> virtio_mmio_init_state_t
// Initialises a virtio device using the mmio mapped to it.
virtio_mmio_init_state_t virtio_init_mmio(
    void* m,
    uint32_t requested_device_id,
    uint32_t (*features_callback)(const volatile void* config, uint32_t features),
    bool (*setup_callback)(volatile virtio_mmio_t* mmio)
);

#define VIRTIO_RING_SIZE (1 << 7)

enum {
    VIRTIO_DESCRIPTOR_FLAG_NEXT = 1,
    VIRTIO_DESCRIPTOR_FLAG_WRITE_ONLY = 2,
    VIRTIO_DESCRIPTOR_FLAG_INDIRECT = 4,
};

typedef struct __attribute__((__packed__)) {
    volatile void* addr;
    uint32_t length;
    uint16_t flags;
    uint16_t next;
} virtio_descriptor_t;

typedef struct __attribute__((__packed__)) {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTIO_RING_SIZE];
    uint16_t event;
    char padding[2];
} virtio_available_t;

typedef struct __attribute__((__packed__)) {
    uint16_t flags;
    uint16_t idx;
    struct __attribute__((__packed__)) {
        uint32_t id;
        uint32_t length;
    } ring[VIRTIO_RING_SIZE];
    uint16_t event;
} virtio_used_t;

typedef struct __attribute__((packed)) {
    volatile uint32_t num;
    volatile uint32_t last_seen_used;
    volatile virtio_descriptor_t* desc;
    volatile virtio_available_t* available;
    volatile virtio_used_t* used;
} virtio_queue_t;

// virtqueue_add_to_device(volatile virtio_mmio_t* mmio, uint32_t) -> virtio_queue_t*
// Adds a virtqueue to a device.
virtio_queue_t* virtqueue_add_to_device(volatile virtio_mmio_t* mmio, uint32_t queue_sel);

// virtqueue_push_descriptor(virtio_queue_t*, uint16_t*) -> volatile virtio_descriptor_t*
// Pushes a descriptor to a queue.
volatile virtio_descriptor_t* virtqueue_push_descriptor(virtio_queue_t* queue, uint16_t* desc_index);

// virtqueue_push_available(virtio_queue_t*, uint16_t) -> void
// Pushes an available descriptor to a queue.
void virtqueue_push_available(virtio_queue_t* queue, uint16_t desc);

// virtqueue_pop_used(virtio_queue_t*) -> volatile virtio_descriptor_t*
// Pops a used descriptor from a queue.
volatile virtio_descriptor_t* virtqueue_pop_used(virtio_queue_t* queue);

#endif /* VIRTIO_H */
