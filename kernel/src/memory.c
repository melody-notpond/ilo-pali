#include <stdbool.h>

#include "console.h"
#include "memory.h"
#include "mmu.h"

extern page_t pages_bottom;
page_t* heap_bottom;

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
    page_t* addr = (page_t*) be_to_le(32 * addr_cell, reg.data);
    size_t len = be_to_le(32 * size_cell, reg.data + addr_cell * 4) - (&pages_bottom - addr);

    size_t page_rc_count = len / PAGE_SIZE / PAGE_SIZE * 2;
    console_printf("[init_pages] page_rc_count: 0x%lx\n", page_rc_count);

    heap_bottom = &pages_bottom + page_rc_count;

    for (uint64_t* clear = (uint64_t*) &pages_bottom; clear < (uint64_t*) heap_bottom; clear++) {
        *clear = 0;
    }
}

// page_ref_count(page_t*) -> uint16_t*
// Returns the reference count for the page as a pointer.
uint16_t* page_ref_count(page_t* page) {
    return (uint16_t*) &pages_bottom + ((intptr_t) page - (intptr_t) heap_bottom) / PAGE_SIZE;
}

// mark_as_used(void*, size_t) -> void
// Marks the given pages as used.
void mark_as_used(void* page, size_t size) {
    uint16_t* p = page_ref_count(page);
    size_t count = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (size_t i = 0; i < count; i++) {
        *p = 1;
        p++;
    }
}

// alloc_pages(size_t) -> void*
// Allocates a number of pages, zeroing out the values.
void* alloc_pages(size_t count) {
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
                    for (size_t i = 0; i < count; i++) {
                        mmu_map(top, page + i, page + i, MMU_BIT_READ | MMU_BIT_WRITE);
                    }
                } else {
                    // hehe bottom
                }

                for (uint64_t* q = (uint64_t*) page; q < (uint64_t*) (page + count); q++) {
                    *q = 0;
                }

                return page;
            }
        }
    }

    console_printf("[alloc_pages] unable to allocate %lx pages\n", count);
    return NULL;
}

// incr_page_ref_count(void*, size_t) -> void
// Increments the reference count of the selected pages.
void incr_page_ref_count(void* page, size_t count) {
    uint16_t* rc = page_ref_count(page);
    for (size_t i = 0; i < count; i++) {
        if (*rc != UINT16_MAX && *rc != 0) {
            *rc += 1;
        }
        rc++;
    }
}

// dealloc_pages(void*, size_t) -> void
// Decrements the reference count of the selected pages.
void dealloc_pages(void* page, size_t count) {
    uint16_t* rc = page_ref_count(page);
    for (size_t i = 0; i < count; i++) {
        if (*rc != 0) {
            *rc -= 1;
        }
        rc++;
    }
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
