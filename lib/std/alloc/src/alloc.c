#include "alloc.h"

// alloc(alloc_t*, size_t) -> void*
// Allocates the given amount of data.
void* alloc(alloc_t* A, size_t size) {
    return A->alloc(size);
}

void* alloc_resize(alloc_t* A, void* ptr, size_t new_size) {
    return A->alloc_resize(ptr, new_size);
}

// dealloc(alloc_t*, void*) -> void
// Deallocates the given data.
void dealloc(alloc_t* A, void* ptr) {
    A->dealloc(ptr);
}

size_t size_of(alloc_t* A, void* ptr) {
    return A->size_of(ptr);
}
