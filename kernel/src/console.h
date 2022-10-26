#ifndef KERNEL_CONSOLE_H
#define KERNEL_CONSOLE_H

#include <stdarg.h>
#include <stddef.h>

// console_clear_lock_unsafe() -> void
// Clears the console lock unsafely.
void console_clear_lock_unsafe();

// console_write(char*, size_t) -> void
// Writes the given substring to the UART.
void console_write(char* s, size_t len);

// console_puts(char*) -> void
// Prints out a string onto the UART.
int console_puts(const char* s);

// console_vprintf(void (*)(char), char*, ...) -> void
// Writes its arguments according to the format string and write function provided. Takes in a va_list.
int console_vprintf(const char* format, va_list va);

// console_printf(char*, ...) -> void
// Writes its arguments to the UART port according to the format string and write function provided.
__attribute__((format(printf, 1, 2))) int console_printf(const char* format, ...);

// console_put_hexdump(void*, unsigned long long)
// Dumps a hexdump onto the UART port.
void console_put_hexdump(void* data, unsigned long long size);

#endif /* KERNEL_CONSOLE_H */
