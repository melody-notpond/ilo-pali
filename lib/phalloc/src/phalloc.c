#include "phalloc.h"

// physical_allocator_options(capability_t*, size_t) -> phalloc_t
// Creates the physical allocation method options.
phalloc_t physical_allocator_options(capability_t* cap, size_t fallback_alloc_size) {
    if (fallback_alloc_size < 65536 + sizeof(struct s_phalloc_bucket))
        fallback_alloc_size = 65536 + sizeof(struct s_phalloc_bucket);
    return (phalloc_t) {
        .cap = *cap,
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

struct s_phalloc_bucket* phalloc_format_unused(virtual_physical_pair_t data, size_t bucket_size, size_t total_size) {
    size_t size = sizeof(struct s_phalloc_bucket) + bucket_size;
    for (size_t i = 0; i + size < total_size; i += size) {
        struct s_phalloc_bucket* bucket = data.virtual_ + i;
        if (i > 0)
            bucket->prev = data.virtual_ + i - size;
        if (i + size < total_size)
            bucket->next = data.virtual_ + i + size;
        bucket->size = bucket_size;
        bucket->physical = data.physical + i + sizeof(struct s_phalloc_bucket);
    }

    return data.virtual_;
}

virtual_physical_pair_t phalloc_alloc_helper(capability_t* cap, size_t size) {
    return alloc_pages_physical(NULL, (size + PAGE_SIZE - 1) / PAGE_SIZE, PERM_READ | PERM_WRITE, cap);
}

void* phalloc_alloc(void* data, uint64_t origin, size_t size) {
    phalloc_t* phalloc = data;

    struct s_phalloc_bucket* bucket = NULL;
    if (size <= 16) {
        if (phalloc->free_16 == NULL) {
            virtual_physical_pair_t unformatted = phalloc_alloc_helper(&phalloc->cap, phalloc->fallback_alloc_size);
            if (unformatted.virtual_ == NULL)
                return NULL;
            phalloc->free_16 = phalloc_format_unused(unformatted, 16, phalloc->fallback_alloc_size);
        }

        bucket = phalloc->free_16;
        phalloc->free_16 = bucket->next;
    } else if (size <= 64) {
        if (phalloc->free_64 == NULL) {
            virtual_physical_pair_t unformatted = phalloc_alloc_helper(&phalloc->cap, phalloc->fallback_alloc_size);
            if (unformatted.virtual_ == NULL)
                return NULL;
            phalloc->free_64 = phalloc_format_unused(unformatted, 64, phalloc->fallback_alloc_size);
        }

        bucket = phalloc->free_64;
        phalloc->free_64 = bucket->next;
    } else if (size <= 256) {
        if (phalloc->free_256 == NULL) {
            virtual_physical_pair_t unformatted = phalloc_alloc_helper(&phalloc->cap, phalloc->fallback_alloc_size);
            if (unformatted.virtual_ == NULL)
                return NULL;
            phalloc->free_256 = phalloc_format_unused(unformatted, 256, phalloc->fallback_alloc_size);
        }

        bucket = phalloc->free_256;
        phalloc->free_256 = bucket->next;
    } else if (size <= 1024) {
        if (phalloc->free_1024 == NULL) {
            virtual_physical_pair_t unformatted = phalloc_alloc_helper(&phalloc->cap, phalloc->fallback_alloc_size);
            if (unformatted.virtual_ == NULL)
                return NULL;
            phalloc->free_1024 = phalloc_format_unused(unformatted, 1024, phalloc->fallback_alloc_size);
        }

        bucket = phalloc->free_1024;
        phalloc->free_1024 = bucket->next;
    } else if (size <= 4096) {
        if (phalloc->free_4096 == NULL) {
            virtual_physical_pair_t unformatted = phalloc_alloc_helper(&phalloc->cap, phalloc->fallback_alloc_size);
            if (unformatted.virtual_ == NULL)
                return NULL;
            phalloc->free_4096 = phalloc_format_unused(unformatted, 4096, phalloc->fallback_alloc_size);
        }

        bucket = phalloc->free_4096;
        phalloc->free_4096 = bucket->next;
    } else if (size <= 16384) {
        if (phalloc->free_16384 == NULL) {
            virtual_physical_pair_t unformatted = phalloc_alloc_helper(&phalloc->cap, phalloc->fallback_alloc_size);
            if (unformatted.virtual_ == NULL)
                return NULL;
            phalloc->free_16384 = phalloc_format_unused(unformatted, 16384, phalloc->fallback_alloc_size);
        }

        bucket = phalloc->free_16384;
        phalloc->free_16384 = bucket->next;
    } else if (size <= 65536) {
        if (phalloc->free_65536 == NULL) {
            virtual_physical_pair_t unformatted = phalloc_alloc_helper(&phalloc->cap, phalloc->fallback_alloc_size);
            if (unformatted.virtual_ == NULL)
                return NULL;
            phalloc->free_65536 = phalloc_format_unused(unformatted, 65536, phalloc->fallback_alloc_size);
        }

        bucket = phalloc->free_65536;
        phalloc->free_65536 = bucket->next;
    } else {
        virtual_physical_pair_t unformatted = phalloc_alloc_helper(&phalloc->cap, size + sizeof(struct s_phalloc_bucket));
        if (unformatted.virtual_ == NULL)
            return NULL;
        bucket = unformatted.virtual_;
        bucket->size = size;
        bucket->physical = unformatted.physical + sizeof(struct s_phalloc_bucket);
    }

    if (bucket == NULL)
        return NULL;

    bucket->next = phalloc->used;
    if (phalloc->used != NULL) {
        phalloc->used->prev = bucket;
    }
    phalloc->used = bucket;
    bucket->prev = NULL;
    return bucket + 1;
}

void phalloc_dealloc(void* data, void* p) {
    if (p == NULL)
        return;

    phalloc_t* phalloc = data;
    struct s_phalloc_bucket* bucket = p;
    bucket--;

    if (bucket->prev != NULL)
        bucket->prev->next = bucket->next;
    if (bucket->next != NULL)
        bucket->next->prev = bucket->prev;
    if (bucket == phalloc->used)
        phalloc->used = phalloc->used->next;
    bucket->prev = NULL;
    bucket->next = NULL;

    if (bucket->size <= 16) {
        phalloc->free_16->prev = bucket;
        bucket->next = phalloc->free_16;
        phalloc->free_16 = bucket;
    } else if (bucket->size <= 64) {
        phalloc->free_64->prev = bucket;
        bucket->next = phalloc->free_64;
        phalloc->free_64 = bucket;
    } else if (bucket->size <= 256) {
        phalloc->free_256->prev = bucket;
        bucket->next = phalloc->free_256;
        phalloc->free_256 = bucket;
    } else if (bucket->size <= 1024) {
        phalloc->free_1024->prev = bucket;
        bucket->next = phalloc->free_1024;
        phalloc->free_1024 = bucket;
    } else if (bucket->size <= 4096) {
        phalloc->free_4096->prev = bucket;
        bucket->next = phalloc->free_4096;
        phalloc->free_4096 = bucket;
    } else if (bucket->size <= 16384) {
        phalloc->free_16384->prev = bucket;
        bucket->next = phalloc->free_16384;
        phalloc->free_16384 = bucket;
    } else if (bucket->size <= 65536) {
        phalloc->free_65536->prev = bucket;
        bucket->next = phalloc->free_65536;
        phalloc->free_65536 = bucket;
    } else {
        dealloc_page(bucket, (bucket->size + PAGE_SIZE - 1) / PAGE_SIZE);
    }
}

void* phalloc_alloc_resize(void* data, uint64_t origin, void* p, size_t new_size) {
    if (p == NULL)
        return NULL;

    struct s_phalloc_bucket* bucket = p;
    bucket--;

    if (bucket->size >= new_size)
        return p;

    void* new = phalloc_alloc(data, origin, new_size);
    if (new == NULL)
        return NULL;
    memcpy(new, p, bucket->size);
    phalloc_dealloc(data, p);
    return new;
}

// create_physical_allocator(phalloc_t*) -> alloc_t
// Creates a free bucket allocator.
alloc_t create_physical_allocator(phalloc_t* phalloc) {
    return (alloc_t) {
        .data = phalloc,
        .alloc = phalloc_alloc,
        .alloc_resize = phalloc_alloc_resize,
        .dealloc = phalloc_dealloc,
    };
}

// phalloc_get_physical(void*) -> uint64_t
// Gets the physical address of an allocated address.
uint64_t phalloc_get_physical(void* virtual) {
    if (virtual == NULL)
        return 0;

    struct s_phalloc_bucket* bucket = virtual;
    bucket--;
    return bucket->physical;
}

// phalloc_get_virtual(phalloc_t*, uint64_t) -> void*
// Returns the virtual address associated with the given physical address. Returns NULL on failure.
void* phalloc_get_virtual(phalloc_t* phalloc, uint64_t physical) {
    struct s_phalloc_bucket* used = phalloc->used;

    while (used) {
        if (used->physical == physical)
            return used + 1;
        used = used->next;
    }

    return NULL;
}
