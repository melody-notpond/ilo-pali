#ifndef FORMAT_H
#define FORMAT_H

#include "core/prelude.h"
#include "iter/string.h"

// vformat(void*, fn(str_t, void*) -> void, char*, va_list) -> void
// Formats the given arguments.
void vformat(void* data, void (*writer)(str_t s, void* data), char* format, va_list va);

// format(void*, fn (str_t, void*) -> void, char*, ...) -> void
// Formats the given arguments.
void format(void* data, void (*writer)(str_t s, void* data), char* format, ...);

#endif /* FORMAT_H */
