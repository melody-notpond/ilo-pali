# example-os
An example bare bones OS template for RISC V.

## Build instructions
Install [the Newlib GNU RISC V toolchain](https://github.com/riscv/riscv-gnu-toolchain#installation-newlib) and do `make` to build. Do `make run` to run. It should print out a bunch of debug information related to OpenSBI and then a single `a`.

## Exiting QEMU
This is the new "how to exit Vi" I guess. To exit, press control+x, unpress those keys, and then press a.

## Debugging
Execute `riscv64-unknown-elf-gdb` and run the following commands:
```
(gdb) symbol-file kernel
(gdb) target remote localhost:1234
```

If you'd like to trace the execution since the beginning, uncomment the `# -S` at the end of the line in the makefile and run. This halts the emulator until a gdb connection is established.


## Resources
 - [OpenSBI docs](https://github.com/riscv/riscv-sbi-doc/blob/master/riscv-sbi.adoc)
 - [RISC V specs](https://riscv.org/technical/specifications/)
 - [RISC V Assembly Tutorial](https://riscv-programming.org/book/riscv-book.html)
