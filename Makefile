TARGET = riscv64-unknown-elf
CC     = $(TARGET)-gcc
GDB    = $(TARGET)-gdb
EMU    = qemu-system-riscv64

CODE = src/

EFLAGS = -machine virt -cpu rv64 -bios opensbi-riscv64-generic-fw_dynamic.bin -m 256m -nographic -global virtio-mmio.force-legacy=false -s
ifdef WAIT_GDB
	EFLAGS += -S
endif

.PHONY: all clean run gdb kernel boot build boot_dir

all: kernel boot

clean:
	-rm -r build/

kernel: build
	$(MAKE) -C kernel/ CC=$(CC)

run:
	$(EMU) $(EFLAGS) -kernel build/kernel -initrd build/initrd

boot: boot_dir
	$(MAKE) -C boot/initd CC=$(CC)

boot_dir: build
	mkdir -p build/boot

build:
	mkdir -p build

idisc: boot
	dd if=/dev/zero of=build/initrd bs=4M count=3
	mkfs.fat -F 16 -n INITRD build/initrd
	mkdir -p mnt
	sudo mount build/initrd mnt/
	sudo cp build/boot/* mnt/
	sudo umount mnt

gdb:
	$(GDB) -q -x kernel.gdb
