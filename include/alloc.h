#ifndef ALLOC_H
#define ALLOC_H

#include "core/prelude.h"

typedef struct {
    void* data;
    void* (*alloc)(void*, uint64_t, size_t);
    void* (*alloc_resize)(void*, uint64_t, void*, size_t new_size);
    void (*dealloc)(void*, void*);
} alloc_t;

typedef struct {
    size_t offset;
    size_t size;
    void* bytes;
} bump_alloc_t;

struct s_free_bucket {
    struct s_free_bucket* next;
    struct s_free_bucket* prev;
    size_t size;
    uint64_t origin;
    uint8_t data[];
};

typedef struct {
    alloc_t* fallback;
    size_t fallback_alloc_size;
    struct s_free_bucket* used;
    struct s_free_bucket* free_16;
    struct s_free_bucket* free_64;
    struct s_free_bucket* free_256;
    struct s_free_bucket* free_1024;
    struct s_free_bucket* free_4096;
    struct s_free_bucket* free_16384;
    struct s_free_bucket* free_65536;
} free_buckets_alloc_t;

extern const alloc_t NULL_ALLOC;

// alloc(alloc_t*, size_t) -> void*
// Allocates the given amount of data.
void* alloc(alloc_t* A, size_t size);

// alloc_resize(alloc_t*, void*, size_t) -> void*
// Resizes the given pointer. Retuns NULL on failure.
void* alloc_resize(alloc_t* A, void* ptr, size_t new_size);

// dealloc(alloc_t*, void*) -> void
// Deallocates the given data.
void dealloc(alloc_t* A, void* ptr);

// bump_allocator_options(size_t, void*) -> bump_alloc_t
// Creates a bump allocator options struct.
bump_alloc_t bump_allocator_options(size_t size, void* bytes);

// create_bump_allocator(bump_alloc_t*) -> alloc_t
// Creates a bump allocator.
alloc_t create_bump_allocator(bump_alloc_t* bump);

// free_buckets_allocator_options(alloc_t*, size_t) -> free_buckets_alloc_t
// Creates the free buckets allocation method options.
free_buckets_alloc_t free_buckets_allocator_options(alloc_t* fallback, size_t fallback_alloc_size);

// create_free_buckets_allocator(free_buckets_alloc_t*) -> alloc_t
// Creates a free bucket allocator.
alloc_t create_free_buckets_allocator(free_buckets_alloc_t* free_buckets);

#endif /* ALLOC_H */
