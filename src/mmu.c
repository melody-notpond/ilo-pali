#include "memory.h"
#include "mmu.h"

#define MMU_UNWRAP(t, v, i) ((mmu_level_##t##_t*) (((v)[i] << 2) & ~0xfff))

#define MMU_BIT_VALID       0x01
#define MMU_BIT_READ        0x02
#define MMU_BIT_WRITE       0x04
#define MMU_BIT_EXECUTE     0x08
#define MMU_BIT_USER        0x10
#define MMU_BIT_GLOBAL      0x20
#define MMU_BIT_ACCESSED    0x40
#define MMU_BIT_DIRTY       0x80

// get_mmu() -> mmu_level_1_t*
// Gets the current value of satp and converts it into a pointer to the mmu table.
mmu_level_1_t* get_mmu() {
    intptr_t mmu;
    asm volatile("csrr %0, satp" : "=r" (mmu));

    if ((mmu & 0x8000000000000000) != 0)
        return (mmu_level_1_t*) ((mmu & 0x00000fffffffffff) << 12);

    return NULL;
}

// set_mmu(mmu_level_1_t*) -> void
// Sets the satp csr to the provided mmu table pointer.
void set_mmu(mmu_level_1_t* top) {
    if (top) {
        uint64_t mmu = 0x8000000000000000 | ((intptr_t) top >> 12);
        asm volatile("csrw satp, %0" : "=r" (mmu));
    } else {
        uint64_t mmu = 0;
        asm volatile("csrw satp, %0" : "=r" (mmu));
    }
}

// create_mmu_table() -> mmu_level_1_t*
// Creates an empty mmu table.
mmu_level_1_t* create_mmu_table() {
    return alloc_pages(1);
}

// mmu_map(mmu_level_1_t*, void*, void*, int) -> void
// Maps the given virtual address to the given physical address.
void mmu_map(mmu_level_1_t* top, void* virtual, void* physical, int flags) {
    intptr_t vpn2 = ((intptr_t) virtual >> 30) & 0x1ff;
    intptr_t vpn1 = ((intptr_t) virtual >> 21) & 0x1ff;
    intptr_t vpn0 = ((intptr_t) virtual >> 12) & 0x1ff;

    if (!MMU_UNWRAP(2, top, vpn2)) {
        top[vpn2] = ((intptr_t) alloc_pages(1)) >> 2 | MMU_BIT_VALID;
    }

    mmu_level_2_t* level2 = MMU_UNWRAP(2, top, vpn2);

    if (!MMU_UNWRAP(3, level2, vpn1)) {
        level2[vpn1] = ((intptr_t) alloc_pages(1)) >> 2 | MMU_BIT_VALID;
    }

    mmu_level_3_t* level3 = MMU_UNWRAP(3, level2, vpn1);

    if (!MMU_UNWRAP(4, level3, vpn0)) {
        level3[vpn0] = ((intptr_t) physical & ~0x03) >> 2 | flags | MMU_BIT_VALID;
    }
}

// mmu_alloc(mmu_level_1_t*, void*, int) -> void*
// Allocates a new page and inserts it into the mmu table, returning the physical address.
void* mmu_alloc(mmu_level_1_t* top, void* virtual, int flags) {
    intptr_t vpn2 = ((intptr_t) virtual >> 30) & 0x1ff;
    intptr_t vpn1 = ((intptr_t) virtual >> 21) & 0x1ff;
    intptr_t vpn0 = ((intptr_t) virtual >> 12) & 0x1ff;

    if (!MMU_UNWRAP(2, top, vpn2)) {
        top[vpn2] = ((intptr_t) alloc_pages(1)) >> 2 | MMU_BIT_VALID;
    }

    mmu_level_2_t* level2 = MMU_UNWRAP(2, top, vpn2);

    if (!MMU_UNWRAP(3, level2, vpn1)) {
        level2[vpn1] = ((intptr_t) alloc_pages(1)) >> 2 | MMU_BIT_VALID;
    }

    mmu_level_3_t* level3 = MMU_UNWRAP(3, level2, vpn1);

    if (!MMU_UNWRAP(4, level3, vpn0)) {
        void* physical = alloc_pages(1);
        mmu_map(top, virtual, physical, flags);
        level3[vpn0] = ((intptr_t) physical & ~0x03) >> 2 | flags | MMU_BIT_VALID;
        return physical;
    }

    return NULL;
}

// mmu_map_range_identity(mmu_level_1_t*, void*, void*, int) -> void
// Maps the given range to itself in the given mmu table.
void mmu_map_range_identity(mmu_level_1_t* top, void* start, void* end, int flags) {
    for (void* p = start; p < end; p += PAGE_SIZE) {
        mmu_map(top, p, p, flags);
    }
}

// identity_map_kernel(fdt_t*, void*, void*) -> void
// Identity maps the kernel in the given mmu table.
void identity_map_kernel(mmu_level_1_t* top, fdt_t* fdt, void* initrd_start, void* initrd_end) {
    extern int text_start;
    extern int data_start;
    extern int ro_data_start;
    extern int sdata_start;
    extern int stack_start;
    extern int stack_top;
    extern int pages_bottom;
    extern page_t* heap_bottom;

    // Map kernel
    mmu_map_range_identity(top, &text_start, &data_start, MMU_BIT_READ | MMU_BIT_EXECUTE | MMU_BIT_GLOBAL);
    mmu_map_range_identity(top, &data_start, &ro_data_start, MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_GLOBAL);
    mmu_map_range_identity(top, &ro_data_start, &data_start, MMU_BIT_READ | MMU_BIT_GLOBAL);
    mmu_map_range_identity(top, &sdata_start, &stack_start, MMU_BIT_READ | MMU_BIT_GLOBAL);
    mmu_map_range_identity(top, &stack_start, &stack_top, MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_GLOBAL);
    mmu_map_range_identity(top, &pages_bottom, heap_bottom, MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_GLOBAL);

    // Map fdt
    mmu_map_range_identity(top, fdt->header, ((void*) fdt->header) + be_to_le(32, fdt->header->totalsize), MMU_BIT_READ);

    // Map initrd
    mmu_map_range_identity(top, initrd_start, initrd_end, MMU_BIT_READ);
}
