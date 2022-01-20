#include "alloc.h"

const alloc_t NULL_ALLOC = { 0 };

// alloc(alloc_t*, size_t) -> void*
// Allocates the given amount of data.
void* alloc(alloc_t* A, size_t size) {
    uint64_t origin;
    asm volatile("mv %0, ra" : "=r" (origin));
    if (A->alloc)
        return A->alloc(A->data, origin, size);
    return NULL;
}

// alloc_resize(alloc_t*, void*, size_t) -> void*
// Resizes the given pointer. Retuns NULL on failure.
void* alloc_resize(alloc_t* A, void* ptr, size_t new_size) {
    uint64_t origin;
    asm volatile("mv %0, ra" : "=r" (origin));
    if (A->alloc_resize)
        return A->alloc_resize(A->data, origin, ptr, new_size);
    return NULL;
}

// dealloc(alloc_t*, void*) -> void
// Deallocates the given data.
void dealloc(alloc_t* A, void* ptr) {
    if (A->dealloc)
        A->dealloc(A->data, ptr);
}

// bump_allocator_options(size_t, void*) -> bump_alloc_t
// Creates a bump allocator options struct.
bump_alloc_t bump_allocator_options(size_t size, void* bytes) {
    return (bump_alloc_t) {
        .offset = 0,
        .size = size,
        .bytes = bytes,
    };
}

void* bump_allocator_alloc(void* data, uint64_t _, size_t size) {
    bump_alloc_t* bump = data;

    if (bump->size < bump->offset + size)
        return NULL;
    void* result = bump->bytes + bump->offset;
    bump->offset += size;
    return result;
}

// create_bump_allocator(bump_alloc_t*) -> alloc_t
// Creates a bump allocator.
alloc_t create_bump_allocator(bump_alloc_t* bump) {
    return (alloc_t) {
        .data = bump,
        .alloc = bump_allocator_alloc,
        .alloc_resize = NULL,
        .dealloc = NULL,
    };
}

// free_buckets_allocator_options(alloc_t*, size_t) -> free_buckets_alloc_t
// Creates the free buckets allocation method options.
free_buckets_alloc_t free_buckets_allocator_options(alloc_t* fallback, size_t fallback_alloc_size) {
    if (fallback_alloc_size < 65536 + sizeof(struct s_free_bucket))
        fallback_alloc_size = 65536 + sizeof(struct s_free_bucket);
    return (free_buckets_alloc_t) {
        .fallback = fallback,
        .fallback_alloc_size = fallback_alloc_size,
        .used = NULL,
        .free_16 = NULL,
        .free_64 = NULL,
        .free_256 = NULL,
        .free_1024 = NULL,
        .free_4096 = NULL,
        .free_16384 = NULL,
        .free_65536 = NULL,
    };
}

struct s_free_bucket* free_buckets_format_unused(void* data, size_t bucket_size, size_t total_size) {
    size_t size = sizeof(struct s_free_bucket) + bucket_size;
    for (size_t i = 0; i + size < total_size; i += size) {
        struct s_free_bucket* bucket = data + i;
        if (i > 0)
            bucket->prev = data + i - size;
        if (i + size < total_size)
            bucket->next = data + i + size;
        bucket->size = bucket_size;
        bucket->origin = 0;
    }

    return data;
}

void* free_buckets_alloc(void* data, uint64_t origin, size_t size) {
    free_buckets_alloc_t* free_buckets = data;

    struct s_free_bucket* bucket = NULL;
    if (size <= 16) {
        if (free_buckets->free_16 == NULL) {
            void* unformatted = alloc(free_buckets->fallback, free_buckets->fallback_alloc_size);
            if (unformatted == NULL)
                return NULL;
            free_buckets->free_16 = free_buckets_format_unused(unformatted, 16, free_buckets->fallback_alloc_size);
        }

        bucket = free_buckets->free_16;
        free_buckets->free_16 = bucket->next;
    } else if (size <= 64) {
        if (free_buckets->free_64 == NULL) {
            void* unformatted = alloc(free_buckets->fallback, free_buckets->fallback_alloc_size);
            if (unformatted == NULL)
                return NULL;
            free_buckets->free_64 = free_buckets_format_unused(unformatted, 64, free_buckets->fallback_alloc_size);
        }

        bucket = free_buckets->free_64;
        free_buckets->free_64 = bucket->next;
    } else if (size <= 256) {
        if (free_buckets->free_256 == NULL) {
            void* unformatted = alloc(free_buckets->fallback, free_buckets->fallback_alloc_size);
            if (unformatted == NULL)
                return NULL;
            free_buckets->free_256 = free_buckets_format_unused(unformatted, 256, free_buckets->fallback_alloc_size);
        }

        bucket = free_buckets->free_256;
        free_buckets->free_256 = bucket->next;
    } else if (size <= 1024) {
        if (free_buckets->free_1024 == NULL) {
            void* unformatted = alloc(free_buckets->fallback, free_buckets->fallback_alloc_size);
            if (unformatted == NULL)
                return NULL;
            free_buckets->free_1024 = free_buckets_format_unused(unformatted, 1024, free_buckets->fallback_alloc_size);
        }

        bucket = free_buckets->free_1024;
        free_buckets->free_1024 = bucket->next;
    } else if (size <= 4096) {
        if (free_buckets->free_4096 == NULL) {
            void* unformatted = alloc(free_buckets->fallback, free_buckets->fallback_alloc_size);
            if (unformatted == NULL)
                return NULL;
            free_buckets->free_4096 = free_buckets_format_unused(unformatted, 4096, free_buckets->fallback_alloc_size);
        }

        bucket = free_buckets->free_4096;
        free_buckets->free_4096 = bucket->next;
    } else if (size <= 16384) {
        if (free_buckets->free_16384 == NULL) {
            void* unformatted = alloc(free_buckets->fallback, free_buckets->fallback_alloc_size);
            if (unformatted == NULL)
                return NULL;
            free_buckets->free_16384 = free_buckets_format_unused(unformatted, 16384, free_buckets->fallback_alloc_size);
        }

        bucket = free_buckets->free_16384;
        free_buckets->free_16384 = bucket->next;
    } else if (size <= 65536) {
        if (free_buckets->free_65536 == NULL) {
            void* unformatted = alloc(free_buckets->fallback, free_buckets->fallback_alloc_size);
            if (unformatted == NULL)
                return NULL;
            free_buckets->free_65536 = free_buckets_format_unused(unformatted, 65536, free_buckets->fallback_alloc_size);
        }

        bucket = free_buckets->free_65536;
        free_buckets->free_65536 = bucket->next;
    } else {
        bucket = alloc(free_buckets->fallback, size + sizeof(struct s_free_bucket));
        if (bucket == NULL)
            return NULL;
        bucket->size = size;
    }

    if (bucket == NULL)
        return NULL;

    bucket->origin = origin;
    bucket->next = free_buckets->used;
    if (free_buckets->used != NULL) {
        free_buckets->used->prev = bucket;
    }
    free_buckets->used = bucket;
    bucket->prev = NULL;
    return bucket + 1;
}

void free_buckets_dealloc(void* data, void* p) {
    if (p == NULL)
        return;

    free_buckets_alloc_t* free_buckets = data;
    struct s_free_bucket* bucket = p;
    bucket--;

    if (bucket->prev != NULL)
        bucket->prev->next = bucket->next;
    if (bucket->next != NULL)
        bucket->next->prev = bucket->prev;
    if (bucket == free_buckets->used)
        free_buckets->used = free_buckets->used->next;
    bucket->prev = NULL;
    bucket->next = NULL;

    if (bucket->size <= 16) {
        free_buckets->free_16->prev = bucket;
        bucket->next = free_buckets->free_16;
        free_buckets->free_16 = bucket;
    } else if (bucket->size <= 64) {
        free_buckets->free_64->prev = bucket;
        bucket->next = free_buckets->free_64;
        free_buckets->free_64 = bucket;
    } else if (bucket->size <= 256) {
        free_buckets->free_256->prev = bucket;
        bucket->next = free_buckets->free_256;
        free_buckets->free_256 = bucket;
    } else if (bucket->size <= 1024) {
        free_buckets->free_1024->prev = bucket;
        bucket->next = free_buckets->free_1024;
        free_buckets->free_1024 = bucket;
    } else if (bucket->size <= 4096) {
        free_buckets->free_4096->prev = bucket;
        bucket->next = free_buckets->free_4096;
        free_buckets->free_4096 = bucket;
    } else if (bucket->size <= 16384) {
        free_buckets->free_16384->prev = bucket;
        bucket->next = free_buckets->free_16384;
        free_buckets->free_16384 = bucket;
    } else if (bucket->size <= 65536) {
        free_buckets->free_65536->prev = bucket;
        bucket->next = free_buckets->free_65536;
        free_buckets->free_65536 = bucket;
    } else {
        dealloc(free_buckets->fallback, bucket);
    }
}

void* free_buckets_alloc_resize(void* data, uint64_t origin, void* p, size_t new_size) {
    if (p == NULL)
        return NULL;

    struct s_free_bucket* bucket = p;
    bucket--;

    if (bucket->size >= new_size)
        return p;

    void* new = free_buckets_alloc(data, origin, new_size);
    if (new == NULL)
        return NULL;
    memcpy(new, p, bucket->size);
    free_buckets_dealloc(data, p);
    return new;
}

// create_free_buckets_allocator(free_buckets_alloc_t*) -> alloc_t
// Creates a free bucket allocator.
alloc_t create_free_buckets_allocator(free_buckets_alloc_t* free_buckets) {
    return (alloc_t) {
        .data = free_buckets,
        .alloc = free_buckets_alloc,
        .alloc_resize = free_buckets_alloc_resize,
        .dealloc = free_buckets_dealloc,
    };
}
