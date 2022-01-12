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
	mkdir -p build
	$(CC) $(CFLAGS) $? -o build/kernel

clean:
	-rm -r build/

run:
	$(EMU) $(EFLAGS) -kernel build/kernel -initrd build/initrd

disc:
	mkdir -p build
	dd if=/dev/zero of=build/initrd bs=4M count=3
	mkfs.fat -F 16 -n INITRD build/initrd
	mkdir -p mnt
	sudo mount build/initrd mnt/
	echo 'echo "uwu" > mnt/uwu.txt' | sudo su
	echo 'echo "initd" > mnt/initd' | sudo su
	sudo umount mnt

gdb:
	$(GDB) -q -x kernel.gdb
