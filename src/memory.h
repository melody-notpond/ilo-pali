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

// alloc_pages(size_t) -> void*
// Allocates a number of pages.
void* alloc_pages(size_t count);

// incr_page_ref_count(page_t*, size_t) -> void
// Increments the reference count of the selected pages.
void incr_page_ref_count(page_t* page, size_t count);

// dealloc_pages(page_t*, size_t) -> void
// Decrements the reference count of the selected pages.
void dealloc_pages(page_t* page, size_t count);

#endif /* MEMORY_H */
