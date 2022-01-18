TARGET = riscv64-unknown-elf
CC     = $(TARGET)-gcc
GDB    = $(TARGET)-gdb
EMU    = qemu-system-riscv64

CODE = src/

EFLAGS = -machine virt -cpu rv64 -bios opensbi-riscv64-generic-fw_dynamic.bin -device virtio-gpu-device -m 256m -global virtio-mmio.force-legacy=false -s -nographic
ifdef WAIT_GDB
	EFLAGS += -S
endif

.PHONY: all clean run gdb kernel boot build boot_dir lib lib_dir

all: kernel boot

clean:
	-rm -r build/

kernel: build
	$(MAKE) -C kernel/

run:
	$(EMU) $(EFLAGS) -kernel build/kernel -initrd build/initrd

boot: boot_dir lib
	$(MAKE) -C boot/initd
	$(MAKE) -C boot/virtd
	cp boot/maps build/boot/

lib: lib_dir
	$(MAKE) -C lib/fat/
	$(MAKE) -C lib/fdt/
	$(MAKE) -C lib/std/alloc/
	$(MAKE) -C lib/std/core/
	$(MAKE) -C lib/std/format/
	$(MAKE) -C lib/std/iter/
	$(MAKE) -C lib/std/join/
	$(MAKE) -C lib/std/syscall/

lib_dir:
	mkdir -p build/lib

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
