# ilo pali
An operating system for RISC V, featuring a small microkernel.

## Build instructions
Install [the Newlib GNU RISC-V toolchain](https://github.com/riscv/riscv-gnu-toolchain#installation-newlib) and do `make` to build. Do `make run` to run. It should print out a bunch of debug information related to OpenSBI and then a single `a`.

Alternatively, you can install Clang and use `make CC=clang`.

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
 - syscalls:
    - `alloc_page(void* addr, size_t count, int permissions) -> void* addr`

        Allocates `count` pages of memory containing addr. If addr is NULL, then it allocates the next available page. Only the init process can pass in non-NULL arguments to addr. Returns NULL on failure. Write and execute cannot both be set at the same time.

    - `dealloc_page(void* addr, size_t count) -> int status`

        Deallocates the page(s) containing the given address. Returns 0 on success and 1 if a page was never allocated by this process.

    - `page_permissions(void* addr, size_t count, int permissions) -> int status`

        Modifies the permissions of the given pages. Returns 0 on success, 1 if the page was never allocated, and 2 if both write and execute were attempted to be set.

        Permissions:
         - READ    - 0b100
         - WRITE   - 0b010
         - EXECUTE - 0b001

    - `spawn(void* exe, size_t exe_size, void** deps, size_t dep_sizes, size_t dep_count, void* args, size_t args_size) -> pid_t child`

        Spawns a process with the given executable binary and dependencies. Returns a pid of -1 on failure.

        The executable may be a valid elf file, as can the dependencies. All data will be copied over to a new set of pages.

    - `kill(pid_t pid) -> int status`

        Kills the given process. Returns 0 on success, 1 if the process does not exist, and 2 if insufficient permissions.

    - `send(bool block, pid_t pid, int type, uint64_t data, uint64_t metadata) -> int status`

        Sends data to the given process. Returns 0 on success, 1 if process does not exist, and 2 if invalid arguments, and 3 if message queue is full. Blocks until the message is sent if block is true. If block is false, then it immediately returns.

        Types:
         - SIGNAL  - 0

            Metadata can be any integer argument for the signal (for example, the size of the requested data).

         - INT     - 1

            Metadata can be set to send a 128 bit integer.

         - POINTER - 2

            Metadata contains the size of the pointer. The kernel will share the pages necessary between processes.

        - DATA - 3

            Metadata contains the size of the data. The kernel will copy the required data between processes. Maximum is 1 page.

    - `recv(bool block, pid_t* pid, int* type, uint64_t* data, uint64_t* metadata) -> int status`

        Blocks until a message is received and deposits the data into the pointers provided. If block is false, then it immediately returns. Returns 0 if message was received and 1 if not.

    - `uart_write(size_t size, void* data) -> void`

        Writes data to the UART port.

    - `spawn_thread(void (*func)(void*), void* data) -> pid_t thread`

        Spawns a thread (a process sharing the same memory as the current process) that executes the given function. Returns -1 on failure.

    - `lock(void* ref, int type, uint64_t value) -> int status`

        Locks the current process until the given condition is true. Returns 0 on success and 1 on invalid arguments, and 2 on unknown error.

        Types:
         - WAIT    - 0

            Waits while the pointer provided is the same as value.

         - WAKE    - 1

            Wakes when the pointer provided is the same as value.

    - `getpid() -> pid_t`

        Gets the pid of the current process.

    - `getuid(pid_t pid) -> uid_t`

        Gets the uid of the given process. Returns -1 if the process doesn't exist.

    - `setuid(pid_t pid, uid_t uid) -> int status`

        Sets the uid of the given process (can only be done by processes with uid = 0). Returns 0 on success, 1 if the process does not exist, and 2 if insufficient permissions.

    - `sleep(size_t seconds, size_t micros) -> time_t current`

        Sleeps for the given amount of time. Returns the current time. Does not interrupt receive handlers or interrupt handlers. If the sleep time passed in is 0, then the syscall returns immediately.

## Resources
 - [OpenSBI docs](https://github.com/riscv/riscv-sbi-doc/blob/master/riscv-sbi.adoc)
 - [RISC-V specs](https://riscv.org/technical/specifications/)
 - [RISC-V assembly tutorial](https://riscv-programming.org/book/riscv-book.html)
