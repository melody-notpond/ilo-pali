TARGET = riscv64-unknown-elf
CC     = clang
GDB    = riscv64-elf-gdb
# GDB  = $(TARGET)-gdb
EMU    = qemu-system-riscv64
CORES  = 1
# CORES= 4
SUPER  = sudo

CODE = src/

EDEVICES = -device virtio-blk-device,scsi=off,drive=root
EFLAGS = -machine virt -cpu rv64 -bios opensbi-riscv64-generic-fw_dynamic.bin -m 256m -global virtio-mmio.force-legacy=false $(EDEVICES) -smp $(CORES) -s
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

kernel: build
	$(MAKE) -C kernel/

run:
	$(EMU) $(EFLAGS) -drive if=none,format=raw,file=build/root.iso,id=root -kernel build/kernel -initrd build/initrd

boot: boot_dir lib
	$(MAKE) -C boot/initd/
	$(MAKE) -C boot/uwu/

root: root_dir lib
	cp root/test.txt build/root/

root_dir:
	mkdir -p build/root/

lib: lib_dir
	$(MAKE) -C lib/c/

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
	$(SUPER) mount build/initrd mnt_boot/
	$(SUPER) cp build/boot/* mnt_boot/
	$(SUPER) umount mnt_boot

rdisc: root
	dd if=/dev/zero of=build/root.iso bs=4M count=256
	mkdir -p mnt_root
	./makefile-helper.sh

gdb:
	$(GDB) -q -x kernel.gdb
