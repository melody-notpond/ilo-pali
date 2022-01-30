TARGET = riscv64-unknown-elf
CC     = $(TARGET)-gcc
GDB    = $(TARGET)-gdb
EMU    = qemu-system-riscv64
CORES  = 4

CODE = src/

EFLAGS = -machine virt -cpu rv64 -bios opensbi-riscv64-generic-fw_dynamic.bin -device virtio-gpu-device -m 256m -global virtio-mmio.force-legacy=false -device virtio-blk-device,scsi=off,drive=root -smp $(CORES) -s
ifdef WAIT_GDB
	EFLAGS += -S
endif
ifdef GRAPHIC
	EFLAGS += -serial stdio
else
	EFLAGS += -nographic
endif

.PHONY: all clean run gdb kernel boot build boot_dir lib lib_dir root root_dir discs idisc rdisc

all: kernel boot root

clean:
	-rm -r build/
	cd boot/fsd && cargo clean

kernel: build
	$(MAKE) -C kernel/

run:
	$(EMU) $(EFLAGS) -drive if=none,format=raw,file=build/root.iso,id=root -kernel build/kernel -initrd build/initrd

boot: boot_dir lib
	cd boot/fsd/ && cargo build --release
	cp boot/fsd/target/riscv64gc-unknown-none-elf/release/fsd build/boot/
	$(MAKE) -C boot/initd/
	$(MAKE) -C boot/virtd/
	$(MAKE) -C boot/virtblock/
	$(MAKE) -C boot/virtgpu/
	cp boot/maps build/boot/

root: root_dir lib
	cp root/test.txt build/root/

root_dir:
	mkdir -p build/root/

lib: lib_dir
	$(MAKE) -C lib/fat/
	$(MAKE) -C lib/fdt/
	$(MAKE) -C lib/std/alloc/
	$(MAKE) -C lib/std/core/
	$(MAKE) -C lib/std/format/
	$(MAKE) -C lib/std/iter/
	$(MAKE) -C lib/std/join/
	$(MAKE) -C lib/std/sync/
	$(MAKE) -C lib/std/syscall/
	$(MAKE) -C lib/phalloc/
	$(MAKE) -C lib/virtio/

lib_dir: build
	mkdir -p build/lib/

boot_dir: build
	mkdir -p build/boot/

build:
	mkdir -p build/

discs: idisc rdisc

idisc: boot
	dd if=/dev/zero of=build/initrd bs=4M count=3
	mkfs.fat -F 16 -n INITRD build/initrd
	mkdir -p mnt_boot
	sudo mount build/initrd mnt_boot/
	sudo cp build/boot/* mnt_boot/
	sudo umount mnt_boot

rdisc: root
	dd if=/dev/zero of=build/root.iso bs=4M count=256
	./makefile-helper.sh

gdb:
	$(GDB) -q -x kernel.gdb
