TARGET = riscv64-unknown-elf
CC     = $(TARGET)-gcc
GDB    = $(TARGET)-gdb
EMU    = qemu-system-riscv64

CODE = src/

CFLAGS = -march=rv64gc -mabi=lp64d -static -mcmodel=medany -fvisibility=hidden -nostdlib -nostartfiles -Tkernel.ld -g -Wall -Wextra
ifeq ($(CC),clang)
	CFLAGS += -target $(TARGET) -mno-relax -Wno-unused-command-line-argument
endif

EFLAGS = -machine virt -cpu rv64 -bios opensbi-riscv64-generic-fw_dynamic.bin -m 256m -nographic -global virtio-mmio.force-legacy=false -s
ifdef WAIT_GDB
	EFLAGS += -S
endif

.PHONY: all clean run gdb

all: $(CODE)*.s $(CODE)*.c
	$(CC) $(CFLAGS) $? -o kernel

clean:
	-rm kernel

run:
	$(EMU) $(EFLAGS) -kernel kernel

gdb:
	$(GDB) -q -x kernel.gdb
