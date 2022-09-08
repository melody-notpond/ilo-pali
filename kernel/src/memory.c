#include <stdatomic.h>
#include <stdbool.h>

#include "console.h"
#include "memory.h"
#include "mmu.h"

extern page_t pages_bottom;
page_t* memory_start;
page_t* heap_bottom;
static atomic_bool mutating_heap = false;

// init_pages(fdt_t*) -> void
// Initialises the pages to be ready for page allocation.
void init_pages(fdt_t* tree) {
    if (tree->header == NULL) {
        console_puts("[init_pages] device tree passed in is invalid");
        return;
    }

    void* root = fdt_path(tree, "/", NULL);
    struct fdt_property addr_cell_prop = fdt_get_property(tree, root, "#address-cells");
    uint32_t addr_cell = be_to_le(32, addr_cell_prop.data);
    struct fdt_property size_cell_prop = fdt_get_property(tree, root, "#size-cells");
    uint32_t size_cell = be_to_le(32, size_cell_prop.data);

    // TODO: support for multiple memory nodes
    void* memory_node = fdt_find(tree, "memory", NULL);
    struct fdt_property reg = fdt_get_property(tree, memory_node, "reg");

    // TODO: multiple memory segments
    memory_start = (page_t*) be_to_le(32 * addr_cell, reg.data);
    size_t len = be_to_le(32 * size_cell, reg.data + addr_cell * 4) - (&pages_bottom - memory_start);

    size_t page_rc_count = len / PAGE_SIZE / PAGE_SIZE * 2;
    console_printf("[init_pages] page_rc_count: 0x%lx\n", page_rc_count);

    heap_bottom = &pages_bottom + page_rc_count;

    for (uint64_t* clear = (uint64_t*) &pages_bottom; clear < (uint64_t*) heap_bottom; clear++) {
        *clear = 0;
    }
}

// get_memory_start() -> void*
// Gets the start of RAM.
void* get_memory_start() {
    return memory_start;
}

// page_ref_count(page_t*) -> uint16_t*
// Returns the reference count for the page as a pointer.
uint16_t* page_ref_count(page_t* page) {
    if (page > heap_bottom)
        return (uint16_t*) &pages_bottom + ((intptr_t) page - (intptr_t) heap_bottom) / PAGE_SIZE;
    return NULL;
}

// mark_as_used(void*, size_t) -> void
// Marks the given pages as used.
void mark_as_used(void* page, size_t size) {
    bool f = false;
    while (!atomic_compare_exchange_weak(&mutating_heap, &f, true)) {
        f = false;
    }

    uint16_t* p = page_ref_count(page);
    if (p == NULL) {
        mutating_heap = false;
        return;
    }

    size_t count = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (size_t i = 0; i < count; i++) {
        *p = 1;
        p++;
    }

    mutating_heap = false;
}

// alloc_pages(size_t) -> void*
// Allocates a number of pages, zeroing out the values.
void* alloc_pages(size_t count) {
    bool f = false;
    while (!atomic_compare_exchange_weak(&mutating_heap, &f, true)) {
        f = false;
    }

    uint16_t* p = (uint16_t*) &pages_bottom;

    for (; p < (uint16_t*) heap_bottom - count; p++) {
        if (!*p) {
            bool enough = true;
            for (uint16_t* q = p + 1; q < p + count; q++) {
                if (*q) {
                    enough = false;
                    break;
                }
            }

            if (enough) {
                for (uint16_t* q = p; q < p + count; q++) {
                    *q = 1;
                }

                page_t* page = ((intptr_t) p - (intptr_t) &pages_bottom) / 2 + heap_bottom;

                mmu_level_1_t* top = get_mmu();
                if (top) {
                    page = kernel_space_phys2virtual(page);
                } else {
                    // hehe bottom
                }

                for (uint64_t* q = (uint64_t*) page; q < (uint64_t*) (page + count); q++) {
                    *q = 0;
                }

                mutating_heap = false;
                if (top)
                    return kernel_space_virt2physical(page);

                return page;
            }
        }
    }

    console_printf("[alloc_pages] unable to allocate %lx pages\n", count);
    mutating_heap = false;
    return NULL;
}

// incr_page_ref_count(void*, size_t) -> void
// Increments the reference count of the selected pages.
void incr_page_ref_count(void* page, size_t count) {
    bool f = false;
    while (!atomic_compare_exchange_weak(&mutating_heap, &f, true)) {
        f = false;
    }

    uint16_t* rc = page_ref_count(page);
    if (rc == NULL) {
        mutating_heap = false;
        return;
    }

    for (size_t i = 0; i < count; i++) {
        if (*rc != UINT16_MAX && *rc != 0) {
            *rc += 1;
        }
        rc++;
    }

    mutating_heap = false;
}

// dealloc_pages(void*, size_t) -> void
// Decrements the reference count of the selected pages.
void dealloc_pages(void* page, size_t count) {
    bool f = false;
    while (!atomic_compare_exchange_weak(&mutating_heap, &f, true)) {
        f = false;
    }

    if (page > (void*) KERNEL_SPACE_OFFSET)
        page = kernel_space_virt2physical(page);
    uint16_t* rc = page_ref_count(page);
    if (rc == NULL) {
        mutating_heap = false;
        return;
    }

    for (size_t i = 0; i < count; i++) {
        if (*rc != 0) {
            *rc -= 1;
        }
        rc++;
    }

    mutating_heap = false;
}

struct s_free_bucket {
    struct s_free_bucket* next;
    struct s_free_bucket* prev;
    size_t size;
    uint64_t origin;
    uint8_t data[];
};

typedef struct {
    atomic_bool mutating;

    struct s_free_bucket* used;
    struct s_free_bucket* free_16;
    struct s_free_bucket* free_64;
    struct s_free_bucket* free_256;
    struct s_free_bucket* free_1024;
    struct s_free_bucket* free_4096;
    struct s_free_bucket* free_16384;
    struct s_free_bucket* free_65536;
} free_buckets_alloc_t;

static free_buckets_alloc_t GLOBAL_ALLOC = { 0 };
static const size_t GLOBAL_ALLOC_FALLBACK_PAGE_COUNT = 65;

struct s_free_bucket* free_buckets_format_unused(void* data, size_t bucket_size, size_t total_size) {
    total_size *= PAGE_SIZE;
    size_t size = sizeof(struct s_free_bucket) + bucket_size;
    for (size_t i = 0; i + size < total_size; i += size) {
        struct s_free_bucket* bucket = data + i;
        if (i > 0)
            bucket->prev = data + i - size;
        else bucket->prev = NULL;
        if (i + size < total_size)
            bucket->next = data + i + size;
        else bucket->next = NULL;
        bucket->size = bucket_size;
        bucket->origin = 0;
    }

    return data;
}

void* free_buckets_alloc(size_t size, uint64_t origin) {
    if (size == 0)
        return NULL;

    bool f = false;
    while (!atomic_compare_exchange_weak(&GLOBAL_ALLOC.mutating, &f, true)) {
        f = false;
    }

    struct s_free_bucket* bucket = NULL;
    if (size <= 16) {
        if (GLOBAL_ALLOC.free_16 == NULL) {
            void* unformatted = kernel_space_phys2virtual(alloc_pages(GLOBAL_ALLOC_FALLBACK_PAGE_COUNT));
            if (unformatted == NULL) {
                GLOBAL_ALLOC.mutating = false;
                return NULL;
            }
            GLOBAL_ALLOC.free_16 = free_buckets_format_unused(unformatted, 16, GLOBAL_ALLOC_FALLBACK_PAGE_COUNT);
        }

        bucket = GLOBAL_ALLOC.free_16;
        GLOBAL_ALLOC.free_16 = bucket->next;
    } else if (size <= 64) {
        if (GLOBAL_ALLOC.free_64 == NULL) {
            void* unformatted = kernel_space_phys2virtual(alloc_pages(GLOBAL_ALLOC_FALLBACK_PAGE_COUNT));
            if (unformatted == NULL) {
                GLOBAL_ALLOC.mutating = false;
                return NULL;
            }
            GLOBAL_ALLOC.free_64 = free_buckets_format_unused(unformatted, 64, GLOBAL_ALLOC_FALLBACK_PAGE_COUNT);
        }

        bucket = GLOBAL_ALLOC.free_64;
        GLOBAL_ALLOC.free_64 = bucket->next;
    } else if (size <= 256) {
        if (GLOBAL_ALLOC.free_256 == NULL) {
            void* unformatted = kernel_space_phys2virtual(alloc_pages(GLOBAL_ALLOC_FALLBACK_PAGE_COUNT));
            if (unformatted == NULL) {
                GLOBAL_ALLOC.mutating = false;
                return NULL;
            }
            GLOBAL_ALLOC.free_256 = free_buckets_format_unused(unformatted, 256, GLOBAL_ALLOC_FALLBACK_PAGE_COUNT);
        }

        bucket = GLOBAL_ALLOC.free_256;
        GLOBAL_ALLOC.free_256 = bucket->next;
    } else if (size <= 1024) {
        if (GLOBAL_ALLOC.free_1024 == NULL) {
            void* unformatted = kernel_space_phys2virtual(alloc_pages(GLOBAL_ALLOC_FALLBACK_PAGE_COUNT));
            if (unformatted == NULL) {
                GLOBAL_ALLOC.mutating = false;
                return NULL;
            }
            GLOBAL_ALLOC.free_1024 = free_buckets_format_unused(unformatted, 1024, GLOBAL_ALLOC_FALLBACK_PAGE_COUNT);
        }

        bucket = GLOBAL_ALLOC.free_1024;
        GLOBAL_ALLOC.free_1024 = bucket->next;
    } else if (size <= 4096) {
        if (GLOBAL_ALLOC.free_4096 == NULL) {
            void* unformatted = kernel_space_phys2virtual(alloc_pages(GLOBAL_ALLOC_FALLBACK_PAGE_COUNT));
            if (unformatted == NULL) {
                GLOBAL_ALLOC.mutating = false;
                return NULL;
            }
            GLOBAL_ALLOC.free_4096 = free_buckets_format_unused(unformatted, 4096, GLOBAL_ALLOC_FALLBACK_PAGE_COUNT);
        }

        bucket = GLOBAL_ALLOC.free_4096;
        GLOBAL_ALLOC.free_4096 = bucket->next;
    } else if (size <= 16384) {
        if (GLOBAL_ALLOC.free_16384 == NULL) {
            void* unformatted = kernel_space_phys2virtual(alloc_pages(GLOBAL_ALLOC_FALLBACK_PAGE_COUNT));
            if (unformatted == NULL) {
                GLOBAL_ALLOC.mutating = false;
                return NULL;
            }
            GLOBAL_ALLOC.free_16384 = free_buckets_format_unused(unformatted, 16384, GLOBAL_ALLOC_FALLBACK_PAGE_COUNT);
        }

        bucket = GLOBAL_ALLOC.free_16384;
        GLOBAL_ALLOC.free_16384 = bucket->next;
    } else if (size <= 65536) {
        if (GLOBAL_ALLOC.free_65536 == NULL) {
            void* unformatted = kernel_space_phys2virtual(alloc_pages(GLOBAL_ALLOC_FALLBACK_PAGE_COUNT));
            if (unformatted == NULL) {
                GLOBAL_ALLOC.mutating = false;
                return NULL;
            }
            GLOBAL_ALLOC.free_65536 = free_buckets_format_unused(unformatted, 65536, GLOBAL_ALLOC_FALLBACK_PAGE_COUNT);
        }

        bucket = GLOBAL_ALLOC.free_65536;
        GLOBAL_ALLOC.free_65536 = bucket->next;
    } else {
        size_t page_count = (size + sizeof(struct s_free_bucket) + PAGE_SIZE - 1) / PAGE_SIZE;
        bucket = kernel_space_phys2virtual(alloc_pages(page_count));
        if (bucket == NULL) {
            GLOBAL_ALLOC.mutating = false;
            return NULL;
        }
        bucket->size = size;
    }

    if (bucket == NULL) {
        GLOBAL_ALLOC.mutating = false;
        return NULL;
    }

    bucket->origin = origin;
    bucket->next = GLOBAL_ALLOC.used;
    if (GLOBAL_ALLOC.used != NULL) {
        GLOBAL_ALLOC.used->prev = bucket;
    }
    GLOBAL_ALLOC.used = bucket;
    bucket->prev = NULL;
    GLOBAL_ALLOC.mutating = false;
    return bucket + 1;
}

void* malloc(size_t size) {
    uint64_t origin;
    //asm volatile("ld %0, 8(fp)" : "=r" (origin));
    asm volatile("mv %0, ra" : "=r" (origin));
    return free_buckets_alloc(size, origin);
}

void free(void* p) {
    if (p == NULL)
        return;

    bool f = false;
    while (!atomic_compare_exchange_weak(&GLOBAL_ALLOC.mutating, &f, true)) {
        f = false;
    }

    struct s_free_bucket* bucket = p;
    bucket--;

    if (bucket->prev != NULL)
        bucket->prev->next = bucket->next;
    if (bucket->next != NULL)
        bucket->next->prev = bucket->prev;
    if (bucket == GLOBAL_ALLOC.used)
        GLOBAL_ALLOC.used = GLOBAL_ALLOC.used->next;
    bucket->prev = NULL;
    bucket->next = NULL;

    if (bucket->size <= 16) {
        if (GLOBAL_ALLOC.free_16)
            GLOBAL_ALLOC.free_16->prev = bucket;
        bucket->next = GLOBAL_ALLOC.free_16;
        GLOBAL_ALLOC.free_16 = bucket;
    } else if (bucket->size <= 64) {
        if (GLOBAL_ALLOC.free_64)
            GLOBAL_ALLOC.free_64->prev = bucket;
        bucket->next = GLOBAL_ALLOC.free_64;
        GLOBAL_ALLOC.free_64 = bucket;
    } else if (bucket->size <= 256) {
        if (GLOBAL_ALLOC.free_256)
            GLOBAL_ALLOC.free_256->prev = bucket;
        bucket->next = GLOBAL_ALLOC.free_256;
        GLOBAL_ALLOC.free_256 = bucket;
    } else if (bucket->size <= 1024) {
        if (GLOBAL_ALLOC.free_1024)
            GLOBAL_ALLOC.free_1024->prev = bucket;
        bucket->next = GLOBAL_ALLOC.free_1024;
        GLOBAL_ALLOC.free_1024 = bucket;
    } else if (bucket->size <= 4096) {
        if (GLOBAL_ALLOC.free_4096)
            GLOBAL_ALLOC.free_4096->prev = bucket;
        bucket->next = GLOBAL_ALLOC.free_4096;
        GLOBAL_ALLOC.free_4096 = bucket;
    } else if (bucket->size <= 16384) {
        if (GLOBAL_ALLOC.free_16384)
            GLOBAL_ALLOC.free_16384->prev = bucket;
        bucket->next = GLOBAL_ALLOC.free_16384;
        GLOBAL_ALLOC.free_16384 = bucket;
    } else if (bucket->size <= 65536) {
        if (GLOBAL_ALLOC.free_65536)
            GLOBAL_ALLOC.free_65536->prev = bucket;
        bucket->next = GLOBAL_ALLOC.free_65536;
        GLOBAL_ALLOC.free_65536 = bucket;
    } else {
        dealloc_pages(bucket, (bucket->size + sizeof(struct s_free_bucket) + PAGE_SIZE - 1) / PAGE_SIZE);
    }

    GLOBAL_ALLOC.mutating = false;
}

void* realloc(void* p, size_t new_size) {
    if (p == NULL)
        return malloc(new_size);

    struct s_free_bucket* bucket = p;
    bucket--;

    if (bucket->size >= new_size)
        return p;

    uint64_t origin;
    asm volatile("mv %0, ra" : "=r" (origin));
    void* new = free_buckets_alloc(new_size, origin);
    if (new == NULL) {
        return NULL;
    }
    memcpy(new, p, bucket->size);

    free(p);
    return new;
}

// debug_free_buckets_alloc(free_buckets_alloc_t*) -> void
// Prints out the used buckets.
void debug_free_buckets_alloc() {
    bool f = false;
    while (!atomic_compare_exchange_weak(&GLOBAL_ALLOC.mutating, &f, true)) {
        f = false;
    }

    console_puts("Starting debug\n");
    struct s_free_bucket const* bucket = GLOBAL_ALLOC.used;
    while (bucket != NULL) {
        console_printf("Unfreed allocation originating from 0x%lx (%lu bytes)\n", bucket->origin, bucket->size);
        bucket = bucket->next;
    }
    console_puts("Ending debug\n");

    GLOBAL_ALLOC.mutating = false;
}

// memcpy(void*, const void*, unsigned long int) -> void*
// Copys the data from one pointer to another.
void* memcpy(void* dest, const void* src, unsigned long int n) {
    unsigned char* d1 = dest;
    const unsigned char* s1 = src;
    unsigned char* end = dest + n;

    for (; d1 < end; d1++, s1++) {
        *d1 = *s1;
    }

    return dest;
}

// memset(void*, int, unsigned long int) -> void*
// Sets a value over a space. Returns the original pointer.
void* memset(void* p, int i, unsigned long int n) {
    unsigned char c = i;
    for (unsigned char* p1 = p; (void*) p1 < p + n; p1++) {
        *p1 = c;
    }
    return p;
}

// memeq(void*, void*, size_t) -> bool
// Returns true if the two pointers have identical data.
bool memeq(void* p, void* q, size_t size) {
    uint8_t* p1 = p;
    uint8_t* q1 = q;

    for (size_t i = 0; i < size; i++, p1++, q1++) {
        if (*p1 != *q1)
            return false;
    }

    return true;
}
