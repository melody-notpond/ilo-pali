# example-os
An example bare bones OS template for RISC-V.

## Build instructions
Install [the Newlib GNU RISC-V toolchain](https://github.com/riscv/riscv-gnu-toolchain#installation-newlib) and do `make` to build. Do `make run` to run. It should print out a bunch of debug information related to OpenSBI and then a single `a`.

Alternatively, you can install Clang and use `make CC=clang`.

## Exiting QEMU
This is the new "how to exit Vi" I guess. To exit, press <kbd>Ctrl</kbd>+<kbd>a</kbd>, unpress those keys, and then press <kbd>x</kbd>.

## Debugging
Execute `make gdb` and then `make run`.

If you'd like to trace the execution since the beginning, use `make run WAIT_GDB=1`. This halts the emulator until a gdb connection is established.

## Resources
 - [OpenSBI docs](https://github.com/riscv/riscv-sbi-doc/blob/master/riscv-sbi.adoc)
 - [RISC-V specs](https://riscv.org/technical/specifications/)
 - [RISC-V assembly tutorial](https://riscv-programming.org/book/riscv-book.html)
