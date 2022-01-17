#include "iter/vec.h"

struct s_vec {
    size_t len;
    size_t cap;
    size_t item_size;
    void* bytes;
    alloc_t* A;
};

struct s_slice {
    size_t len;
    size_t item_size;
    void* bytes;
};

// empty_vec(alloc_t*, size_t) -> vec_t
// Creates an empty vec. Note that the vec macro is more convenient to use.
vec_t empty_vec(alloc_t* A, size_t item_size) {
    return (vec_t) {
        .len = 0,
        .cap = 0,
        .item_size = item_size,
        .bytes = NULL,
        .A = A,
    };
}

// vec_from_slice(alloc_t*, slice_t) -> vec_t
// Creates a vec from a slice.
vec_t vec_from_slice(alloc_t* A, slice_t slice) {
    vec_t v = {
        .len = slice.len,
        .cap = slice.len,
        .item_size = slice.item_size,
        .bytes = alloc(A, slice.len),
        .A = A,
    };
    memcpy(v.bytes, slice.bytes, slice.len);
    return v;
}

// slice_from_vec(vec_t*) -> slice_t
// Creates a slice from a vec.
slice_t slice_from_vec(vec_t* v) {
    return (slice_t) {
        .len = v->len,
        .item_size = v->item_size,
        .bytes = v->bytes,
    };
}

// slice_slice(slice_t, size_t, size_t) -> slice_t
// Creates a slice from another slice.
slice_t slice_slice(slice_t s, size_t start, size_t end) {
    if (start > end || start > s.len * s.item_size || end > s.len * s.item_size)
        return (slice_t) { 0 };

    return (slice_t) {
        .len = (end - start) * s.item_size,
        .item_size = s.item_size,
        .bytes = s.bytes + start,
    };
}

// vec_get(vec_t*, size_t) -> void*
// Gets the item at the index from the vec. Returns NULL if the index is out of bounds.
void* vec_get(vec_t* v, size_t i) {
    if (i * v->item_size >= v->len)
        return NULL;
    return v->bytes + i * v->item_size;
}

// slice_get(slice_t, size_t) -> void*
// Gets the item at the index from the slice. Returns NULL if the index is out of bounds.
void* slice_get(slice_t s, size_t i) {
    if (i * s.item_size >= s.len)
        return NULL;
    return s.bytes + i * s.item_size;
}

// vec_push(vec_t*, void*) -> void
// Pushes an item onto a vec.
void vec_push(vec_t* v, void* item) {
    if (v->len + v->item_size > v->cap) {
        if (v->cap == 0)
            v->cap = v->item_size * 8;
        else v->cap <<= 1;
        v->bytes = alloc_resize(v->A, v->bytes, v->cap);
    }
    memcpy(v->bytes + v->len * v->item_size, item, v->item_size);
    v->len += v->item_size;
}

// dealloc_vec(vec_t*) -> void
// Deallocates a vec.
void dealloc_vec(vec_t* v) {
    dealloc(v->A, v->bytes);
    v->len = 0;
    v->cap = 0;
    v->item_size = 0;
    v->bytes = NULL;
}
