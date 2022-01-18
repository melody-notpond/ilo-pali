#ifndef JOIN_H
#define JOIN_H

#include "alloc.h"

#define PAGE_ALLOC_PAGE_SIZE (PAGE_SIZE - sizeof(size_t))

extern const alloc_t PAGE_ALLOC;

// uart_printf(char*, ...)
// Prints data to the uart.
void uart_printf(char* format, ...);

// debug_free_buckets_alloc(free_buckets_alloc_t*) -> void
// Prints out the used buckets.
void debug_free_buckets_alloc(free_buckets_alloc_t* free_buckets);

#endif /* JOIN_H */
