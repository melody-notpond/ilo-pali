# ilo pali
An operating system for RISC V, featuring a small microkernel.

## Build instructions
Install [the Newlib GNU RISC-V toolchain](https://github.com/riscv/riscv-gnu-toolchain#installation-newlib) and do `make` to build. Do `make idisc` to build the initrd disc. Do `make run` to run.

Alternatively, you can install Clang and use `make CC=clang` and `make idisc CC=clang`.

## Exiting QEMU
This is the new "how to exit Vi" I guess. To exit, press <kbd>Ctrl</kbd>+<kbd>a</kbd>, unpress those keys, and then press <kbd>x</kbd>.

## Debugging
Execute `make gdb` and then `make run`.

If you'd like to trace the execution since the beginning, use `make run WAIT_GDB=1`. This halts the emulator until a gdb connection is established.

## Features
ilo pali microkernel features:
 - fdt driver
 - uart driver
 - initrd driver (read only)
 - memory management
 - process management
 - like 15 syscalls

## Resources
 - [OpenSBI docs](https://github.com/riscv/riscv-sbi-doc/blob/master/riscv-sbi.adoc)
 - [RISC-V specs](https://riscv.org/technical/specifications/)
 - [RISC-V assembly tutorial](https://riscv-programming.org/book/riscv-book.html)
