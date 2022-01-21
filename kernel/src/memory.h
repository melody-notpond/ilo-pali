#ifndef MEMORY_H
#define MEMORY_H

#define PAGE_SIZE 4096

#include <stddef.h>
#include <stdint.h>

#include "fdt.h"

typedef uint8_t page_t[PAGE_SIZE];

// init_pages(fdt_t*) -> void
// Initialises the pages to be ready for page allocation.
void init_pages(fdt_t* tree);

// get_memory_start() -> void*
// Gets the start of RAM.
void* get_memory_start();

// mark_as_used(void*, size_t) -> void
// Marks the given pages as used.
void mark_as_used(void* page, size_t size);

// alloc_pages(size_t) -> void*
// Allocates a number of pages.
void* alloc_pages(size_t count);

// incr_page_ref_count(void*, size_t) -> void
// Increments the reference count of the selected pages.
void incr_page_ref_count(void* page, size_t count);

// dealloc_pages(void*, size_t) -> void
// Decrements the reference count of the selected pages.
void dealloc_pages(void* page, size_t count);

void* malloc(size_t size);
void* realloc(void* p, size_t new_size);
void free(void* p);

// debug_free_buckets_alloc(free_buckets_alloc_t*) -> void
// Prints out the used buckets.
void debug_free_buckets_alloc();

// memcpy(void*, const void*, unsigned long int) -> void*
// Copys the data from one pointer to another.
void* memcpy(void* dest, const void* src, unsigned long int n);

// memset(void*, int, unsigned long int) -> void*
// Sets a value over a space. Returns the original pointer.
void* memset(void* p, int i, unsigned long int n);

#endif /* MEMORY_H */
