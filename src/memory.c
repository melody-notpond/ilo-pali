#include <stdbool.h>

#include "console.h"
#include "memory.h"

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

    for (volatile uint64_t* clear = (uint64_t*) &pages_bottom; clear < (uint64_t*) heap_bottom; clear++) {
        *clear = 0;
    }
}

// page_ref_count(page_t*) -> uint16_t*
// Returns the reference count for the page as a pointer.
uint16_t* page_ref_count(page_t* page) {
    return (uint16_t*) (&pages_bottom + (page - heap_bottom) / PAGE_SIZE * 2);
}

// mark_as_used(page_t*, size_t) -> void
// Marks the given pages as used.
void mark_as_used(page_t* page, size_t count) {
    uint16_t* p = page_ref_count(page);

    for (size_t i = 0; i < count; i++) {
        *p = 1;
        p++;
    }
}

// alloc_pages(size_t) -> void*
// Allocates a number of pages.
void* alloc_pages(size_t count) {
    uint16_t* p = (uint16_t*) &pages_bottom;

    for (; p < (uint16_t*) heap_bottom; p++) {
        if (!*p) {
            bool enough = true;
            for (uint16_t* q = p; q < p + count; q++) {
                if (*q) {
                    enough = false;
                    break;
                }
            }

            if (enough) {
                for (uint16_t* q = p; q < p + count; q++) {
                    *q = 1;
                }

                page_t* page = ((page_t*) p - &pages_bottom) / 2 * PAGE_SIZE + heap_bottom;
                return page;
            }
        }
    }

    console_printf("[alloc_pages] unable to allocate %lx pages\n", count);
    return NULL;
}

// incr_page_ref_count(page_t*, size_t) -> void
// Increments the reference count of the selected pages.
void incr_page_ref_count(page_t* page, size_t count) {
    uint16_t* rc = page_ref_count(page);
    for (size_t i = 0; i < count; i++) {
        if (*rc != UINT16_MAX) {
            *rc += 1;
        }
        rc++;
    }
}

// dealloc_pages(page_t*, size_t) -> void
// Decrements the reference count of the selected pages.
void dealloc_pages(page_t* page, size_t count) {
    uint16_t* rc = page_ref_count(page);
    for (size_t i = 0; i < count; i++) {
        if (*rc != 0) {
            *rc -= 1;
        }
        rc++;
    }
}
