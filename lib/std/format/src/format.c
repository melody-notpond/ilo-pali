#include "format.h"

// vformat(void*, fn(str_t, void*) -> void, char*, va_list) -> void
// Formats the given arguments.
void vformat(void* data, void (*writer)(str_t s, void* data), char* format, va_list va) {
    size_t start = 0;
    size_t i;
    for (i = 0; format[i]; i++) {
        if (format[i] == '%') {
            writer((str_t) {
                .len = i - start,
                .bytes = format + start,
            }, data);

            switch (format[++i]) {
                case 's': {
                    char* s = va_arg(va, char*);
                    size_t size;
                    for (size = 0; s[size]; size++);
                    writer((str_t) {
                        .len = size,
                        .bytes = s,
                    }, data);
                    break;
                }

                case 'S':
                    writer(va_arg(va, str_t), data);
                    break;

                case 'c': {
                    char c = va_arg(va, uint64_t);
                    writer((str_t) {
                        .len = 1,
                        .bytes = &c,
                    }, data);
                    break;
                }

                case 'i': {
                    int64_t i = va_arg(va, int64_t);
                    bool neg = i < 0;
                    if (neg)
                        i = -i;
                    if (i == 0) {
                        writer((str_t) {
                            .len = 1,
                            .bytes = "0",
                        }, data);
                        break;
                    }

                    static const size_t buffer_size = 20;
                    char buffer[buffer_size];
                    size_t len = 0;
                    while (i != 0) {
                        len++;
                        buffer[buffer_size - len] = '0' + i % 10;
                        i /= 10;
                    }

                    if (neg) {
                        len++;
                        buffer[buffer_size - len] = '-';
                    }

                    writer((str_t) {
                        .len = len,
                        .bytes = buffer + buffer_size - len,
                    }, data);
                    break;
                }

                case 'u': {
                    uint64_t i = va_arg(va, uint64_t);
                    if (i == 0) {
                        writer((str_t) {
                            .len = 1,
                            .bytes = "0",
                        }, data);
                        break;
                    }

                    static const size_t buffer_size = 20;
                    char buffer[buffer_size];
                    size_t len = 0;
                    while (i != 0) {
                        len++;
                        buffer[buffer_size - len] = '0' + i % 10;
                        i /= 10;
                    }
                    writer((str_t) {
                        .len = len,
                        .bytes = buffer + buffer_size - len,
                    }, data);
                    break;
                }

                case 'x': {
                    uint64_t i = va_arg(va, uint64_t);
                    if (i == 0) {
                        writer((str_t) {
                            .len = 1,
                            .bytes = "0",
                        }, data);
                        break;
                    }

                    static const size_t buffer_size = 16;
                    char buffer[buffer_size];
                    size_t len = 0;
                    while (i != 0) {
                        len++;
                        buffer[buffer_size - len] = "0123456789abcdef"[i % 16];
                        i /= 16;
                    }
                    writer((str_t) {
                        .len = len,
                        .bytes = buffer + buffer_size - len,
                    }, data);
                    break;
                }

                case 'X': {
                    uint64_t i = va_arg(va, uint64_t);
                    if (i == 0) {
                        writer((str_t) {
                            .len = 1,
                            .bytes = "0",
                        }, data);
                        break;
                    }

                    static const size_t buffer_size = 16;
                    char buffer[buffer_size];
                    size_t len = 0;
                    while (i != 0) {
                        len++;
                        buffer[buffer_size - len] = "0123456789ABCDEF"[i % 16];
                        i /= 16;
                    }
                    writer((str_t) {
                        .len = len,
                        .bytes = buffer + buffer_size - len,
                    }, data);
                    break;
                }

                case 'p': {
                    intptr_t p = (intptr_t) va_arg(va, void*);
                    char buffer[18];
                    buffer[0] = '0';
                    buffer[1] = 'x';
                    for (size_t i = 15 * 4; i; i -= 4) {
                        buffer[17 - i / 4] = "0123456789abcdef"[((p & (0xf << i)) >> i) & 0xf];
                    }
                    buffer[17] = "0123456789abcdef"[p & 0xf];
                    writer((str_t) {
                        .len = 18,
                        .bytes = buffer,
                    }, data);
                    break;
                }

                default:
                    break;
            }

            i++;
            start = i;
        }
    }

    writer((str_t) {
        .len = i - start,
        .bytes = format + start,
    }, data);
}

// format(void*, fn(str_t, void*) -> void, char*, ...) -> void
// Formats the given arguments.
void format(void* data, void (*writer)(str_t s, void* data), char* format, ...) {
    va_list va;
    va_start(va, format);
    vformat(data, writer, format, va);
    va_end(va);
}
