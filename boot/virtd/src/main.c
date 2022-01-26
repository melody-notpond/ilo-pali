#include "iter/string.h"
#include "syscalls.h"
#include "join.h"
#include "virtio.h"

void _start(void* args, size_t arg_size, uint64_t cap_high, uint64_t cap_low) {
    uart_printf("spawned virtd!\n");

    capability_t initd = ((capability_t) cap_high) << 64 | (capability_t) cap_low;
    uint64_t* args64 = args;
    size_t mmio_size = args64[1];
    volatile virtio_mmio_t* mmio = alloc_pages_physical((void*) args64[0], mmio_size / PAGE_SIZE, PERM_READ | PERM_WRITE, &initd).virtual_;

    pid_t pid;
    int type;
    uint64_t data;
    uint64_t meta;

    capability_t subdriver;
    switch (mmio->device_id) {
        case 0:
            uart_printf("no device\n");
            kill(getpid());
            break;

        case 2: {
            uart_printf("block device found!\n");
            str_t s = S("virtblock");
            send(true, &initd, MSG_TYPE_DATA, (uint64_t) s.bytes, s.len);
            recv(true, &initd, &pid, &type, &data, &meta);
            pid_t p = data;
            recv(true, &initd, &pid, &type, &data, &meta);
            subdriver = (capability_t) meta << 64 | (capability_t) data;
            transfer_capability(&initd, p);
            send(true, &subdriver, MSG_TYPE_INT, initd & 0xffffffffffffffff, initd >> 64);
            send(true, &subdriver, MSG_TYPE_INT, args64[2], 0);
            send(true, &subdriver, MSG_TYPE_POINTER, (uint64_t) mmio, mmio_size);
            break;
        }

        case 16: {
            uart_printf("gpu device found!\n");
            str_t s = S("virtgpu");
            send(true, &initd, MSG_TYPE_DATA, (uint64_t) s.bytes, s.len);
            recv(true, &initd, &pid, &type, &data, &meta);
            pid_t p = data;
            recv(true, &initd, &pid, &type, &data, &meta);
            subdriver = (capability_t) meta << 64 | (capability_t) data;
            transfer_capability(&initd, p);
            send(true, &subdriver, MSG_TYPE_INT, initd & 0xffffffffffffffff, initd >> 64);
            send(true, &subdriver, MSG_TYPE_INT, args64[2], 0);
            send(true, &subdriver, MSG_TYPE_POINTER, (uint64_t) mmio, mmio_size);
            break;
        }

        default:
            uart_printf("unknown device id %x\n", mmio->device_id);
            kill(getpid());
            break;
    }

    recv(true, &subdriver, &pid, &type, &data, &meta);
    kill(getpid());
}
