#ifndef PHALLOC_H
#define PHALLOC_H

#include "core/prelude.h"
#include "alloc.h"
#include "syscalls.h"

struct s_phalloc_bucket {
    struct s_phalloc_bucket* next;
    struct s_phalloc_bucket* prev;
    size_t size;
    uint64_t physical;
    uint8_t data[];
};

typedef struct {
    capability_t cap;
    size_t fallback_alloc_size;
    struct s_phalloc_bucket* used;
    struct s_phalloc_bucket* free_16;
    struct s_phalloc_bucket* free_64;
    struct s_phalloc_bucket* free_256;
    struct s_phalloc_bucket* free_1024;
    struct s_phalloc_bucket* free_4096;
    struct s_phalloc_bucket* free_16384;
    struct s_phalloc_bucket* free_65536;
} phalloc_t;

// physical_allocator_options(capability_t*, size_t) -> phalloc_t
// Creates the physical allocation method options.
phalloc_t physical_allocator_options(capability_t* cap, size_t fallback_alloc_size);

// create_physical_allocator(phalloc_t*) -> alloc_t
// Creates a free bucket allocator.
alloc_t create_physical_allocator(phalloc_t* phalloc);

// phalloc_get_physical(void*) -> uint64_t
// Gets the physical address of an allocated address.
uint64_t phalloc_get_physical(void* virtual);

// phalloc_get_virtual(phalloc_t*, uint64_t) -> void*
// Returns the virtual address associated with the given physical address. Returns NULL on failure.
void* phalloc_get_virtual(phalloc_t* phalloc, uint64_t physical);

#endif /* PHALLOC_H */
