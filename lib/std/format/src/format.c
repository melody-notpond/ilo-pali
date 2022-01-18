#include "format.h"

void format(void* data, void (*writer)(char* s, void* data), char* format, ...) {
    va_list va;
    va_start(va, format);



    va_end(va);
}

