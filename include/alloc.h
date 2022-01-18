#ifndef ALLOC_H
#define ALLOC_H

#include <stddef.h>

typedef struct {
    void* (*alloc)(size_t);
    void* (*alloc_resize)(void*, size_t new_size);
    void (*dealloc)(void*);
    size_t (*size_of)(void*);
} alloc_t;

// alloc(alloc_t*, size_t) -> void*
// Allocates the given amount of data.
void* alloc(alloc_t* A, size_t size);

void* alloc_resize(alloc_t* A, void* ptr, size_t new_size);

// dealloc(alloc_t*, void*) -> void
// Deallocates the given data.
void dealloc(alloc_t* A, void* ptr);

size_t size_of(alloc_t* A, void* ptr);

#endif /* ALLOC_H */
