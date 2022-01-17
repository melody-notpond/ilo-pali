#ifndef VEC_H
#define VEC_H

#include "alloc.h"
#include "core/prelude.h"

#define vec(A, T) (empty_vec(A, sizeof(T)))

// Represents a vector.
typedef struct s_vec vec_t;

// Represents a slice.
typedef struct s_slice slice_t;

// empty_vec(alloc_t*, size_t) -> vec_t
// Creates an empty vec. Note that the vec macro is more convenient to use.
vec_t empty_vec(alloc_t* A, size_t item_size);

// vec_from_slice(alloc_t*, slice_t) -> vec_t
// Creates a vec from a slice.
vec_t vec_from_slice(alloc_t* A, slice_t slice);

// slice_from_vec(vec_t*) -> slice_t
// Creates a slice from a vec.
slice_t slice_from_vec(vec_t* v);

// slice_slice(slice_t, size_t, size_t) -> slice_t
// Creates a slice from another slice.
slice_t slice_slice(slice_t s, size_t start, size_t end);

// vec_get(vec_t*, size_t) -> void*
// Gets the item at the index from the vec. Returns NULL if the index is out of bounds.
void* vec_get(vec_t* v, size_t i);

// slice_get(slice_t, size_t) -> void*
// Gets the item at the index from the slice. Returns NULL if the index is out of bounds.
void* slice_get(slice_t s, size_t i);

// vec_push(vec_t*, void*) -> void
// Pushes an item onto a vec.
void vec_push(vec_t* v, void* item);

// dealloc_vec(vec_t*) -> void
// Deallocates a vec.
void dealloc_vec(vec_t* v);

#endif /* VEC_H */
