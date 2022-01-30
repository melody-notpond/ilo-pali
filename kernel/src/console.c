#include <stdatomic.h>
#include <stdbool.h>

#include "console.h"
#include "opensbi.h"

atomic_bool console_lock = false;

// console_write(char*, size_t) -> void
// Writes the given substring to the UART.
void console_write(char* s, size_t len) {
    bool f = false;
    while (!atomic_compare_exchange_weak(&console_lock, &f, true)) {
        f = false;
    }

    for (size_t i = 0; i < len; i++) {
        sbi_console_putchar(s[i]);
    }

    console_lock = false;
}

// console_puts(char*) -> void
// Prints out a string onto the UART.
int console_puts(const char* s) {
    bool f = false;
    while (!atomic_compare_exchange_weak(&console_lock, &f, true)) {
        f = false;
    }

    while (*s) {
        sbi_console_putchar(*s);
        s++;
    }

    console_lock = false;
    return 0;
}

typedef enum {
    SIZE_INT,
    SIZE_LONG,
    SIZE_LONG_LONG
} console_printf_size_t;

// console_vprintf(void (*)(char), char*, ...) -> void
// Writes its arguments according to the format string and write function provided. Takes in a va_list.
int console_vprintf(const char* format, va_list va) {
    bool f = false;
    while (!atomic_compare_exchange_weak(&console_lock, &f, true)) {
        f = false;
    }

    for (; *format; format++) {
        // Formatters
        if (*format == '%') {
            format++;

            // Determine the type of the formatter
            console_printf_size_t size = SIZE_INT;
            switch (*format) {
                case 'c':
                    sbi_console_putchar(va_arg(va, int));
                    break;

                case 'p': {
                    unsigned long long p = (unsigned long long) va_arg(va, void*);
                    sbi_console_putchar('0');
                    sbi_console_putchar('x');
                    if (p == 0) {
                        sbi_console_putchar('0');
                        break;
                    }
                    static const size_t buffer_size = 16;
                    char buffer[buffer_size];
                    size_t len = 0;
                    while (p != 0) {
                        len++;
                        buffer[buffer_size - len] = "0123456789abcdef"[p % 16];
                        p /= 16;
                    }

                    for (size_t i = buffer_size - len; i < buffer_size; i++) {
                        sbi_console_putchar(buffer[i]);
                    }
                    break;
                }

                case 's': {
                    char* s = va_arg(va, char*);
                    for (; *s; s++) {
                        sbi_console_putchar(*s);
                    }
                    break;
                }

                case 'l':
                    size = SIZE_LONG;
                    format++;
                    if (*format == 'l') {
                        format++;
                        size = SIZE_LONG_LONG;
                    }

                    if (*format != 'x') {
                        switch (size) {
                            case SIZE_INT:
                                va_arg(va, unsigned int);
                                break;
                            case SIZE_LONG:
                                va_arg(va, unsigned long);
                                break;
                            case SIZE_LONG_LONG:
                                va_arg(va, unsigned long long);
                                break;
                        }
                        format--;
                        break;
                    }

                    goto console_printf_fallthrough;

                case 'x':
console_printf_fallthrough: {
                    unsigned long long x;
                    switch (size) {
                        case SIZE_INT:
                            x = va_arg(va, unsigned int);
                            break;
                        case SIZE_LONG:
                            x = va_arg(va, unsigned long);
                            break;
                        case SIZE_LONG_LONG:
                            x = va_arg(va, unsigned long long);
                            break;
                    }

                    if (x == 0) {
                        sbi_console_putchar('0');
                        break;
                    }

                    static const size_t buffer_size = 16;
                    char buffer[buffer_size];
                    size_t len = 0;
                    while (x != 0) {
                        len++;
                        buffer[buffer_size - len] = "0123456789abcdef"[x % 16];
                        x /= 16;
                    }

                    for (size_t i = buffer_size - len; i < buffer_size; i++) {
                        sbi_console_putchar(buffer[i]);
                    }

                    break;
                }

                case '%':
                    sbi_console_putchar('%');
                    break;

                default:
                    break;
            }

        // Regular characters
        } else {
            sbi_console_putchar(*format);
        }
    }

    console_lock = false;
    return 0;
}

// console_printf(char*, ...) -> void
// Writes its arguments to the UART port according to the format string and write function provided.
__attribute__((format(printf, 1, 2))) int console_printf(const char* format, ...) {
    va_list va;
    va_start(va, format);
    console_vprintf(format, va);
    va_end(va);
    return 0;
}

// console_log_16(unsigned long long) -> unsigned int
// Gets the base 16 log of a number as an int.
unsigned int console_log_16(unsigned long long n) {
    unsigned int i = 0;
    while (n) {
        n >>= 4;
        i++;
    }
    return i;
}

// console_put_hexdump(void*, unsigned long long)
// Dumps a hexdump onto the UART port.
void console_put_hexdump(void* data, unsigned long long size) {
    bool f = false;
    while (!atomic_compare_exchange_weak(&console_lock, &f, true)) {
        f = false;
    }

    unsigned int num_zeros = console_log_16(size);
    unsigned char* data_char = (unsigned char*) data;

    for (unsigned long long i = 0; i < (size + 15) / 16; i++) {
        // Print out buffer zeroes
        unsigned int num_zeros_two = num_zeros - console_log_16(i) - 1;
        for (unsigned int j = 0; j < num_zeros_two; j++) {
            console_printf("%x", 0);
        }

        // Print out label
        console_printf("%llx    ", i * 16);

        // Print out values
        for (int j = 0; j < 16; j++) {
            unsigned long long index = i * 16 + j;

            // Skip values if the index is greater than the number of values to dump
            if (index >= size)
                console_puts("   ");
            else {
                // Print out the value
                if (data_char[index] < 16)
                    console_printf("%x", 0);
                console_printf("%x ", data_char[index]);
            }
        }

        console_puts("    |");

        // Print out characters
        for (int j = 0; j < 16; j++) {
            unsigned long long index = i * 16 + j;

            // Skip characters if the index is greater than the number of characters to dump
            if (index >= size)
                sbi_console_putchar('.');

            // Print out printable characters
            else if (32 <= data_char[index] && data_char[index] < 127)
                sbi_console_putchar(data_char[index]);

            // Nonprintable characters are represented by a period (.)
            else
                sbi_console_putchar('.');
        }

        console_puts("|\n");
    }

    console_lock = false;
}

