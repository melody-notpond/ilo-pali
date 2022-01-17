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
inline void* alloc(alloc_t* A, size_t size) {
    return A->alloc(size);
}

inline void* alloc_resize(alloc_t* A, void* ptr, size_t new_size) {
    return A->alloc_resize(ptr, new_size);
}

// dealloc(alloc_t*, void*) -> void
// Deallocates the given data.
inline void dealloc(alloc_t* A, void* ptr) {
    A->dealloc(ptr);
}

inline size_t size_of(alloc_t* A, void* ptr) {
    return A->size_of(ptr);
}

#endif /* ALLOC_H */
