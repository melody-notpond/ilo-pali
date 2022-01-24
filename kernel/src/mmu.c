#include "memory.h"
#include "mmu.h"
#include <stdbool.h>

// kernel_space_virt2physical(void*) -> void*
// Converts a kernel space virtual address into a physical address.
void* kernel_space_virt2physical(void* virtual) {
    return virtual - KERNEL_SPACE_OFFSET;
}

// kernel_space_phys2virtual(void*) -> void*
// Converts a physical address into a kernel space virtual address.
void* kernel_space_phys2virtual(void* physical) {
    return physical + KERNEL_SPACE_OFFSET;
}

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
        mmu_level_1_t* current = get_mmu();
        if (current && current != top) {
            top = kernel_space_phys2virtual(top);
            current = kernel_space_phys2virtual(current);

            for (size_t i = PAGE_SIZE / 2 / sizeof(void*); i < PAGE_SIZE / sizeof(void*); i++) {
                top[i] = current[i];
            }

            top = kernel_space_virt2physical(top);
        }

        uint64_t mmu = 0x8000000000000000 | ((intptr_t) top >> 12);
        asm volatile("csrw satp, %0" : "=r" (mmu));
        asm volatile("sfence.vma");
    } else {
        uint64_t mmu = 0;
        asm volatile("csrw satp, %0" : "=r" (mmu));
        asm volatile("sfence.vma");
    }
}

// flush_mmu() -> void
// Flushes the table buffer or whatever its called.
void flush_mmu() {
    asm volatile("sfence.vma");
}

// create_mmu_table() -> mmu_level_1_t*
// Creates an empty mmu table.
mmu_level_1_t* create_mmu_table() {
    mmu_level_1_t* top = alloc_pages(1);
    mmu_level_1_t* current = get_mmu();

    if (current != NULL) {
        top = kernel_space_phys2virtual(top);
        current = kernel_space_phys2virtual(current);
        for (size_t i = PAGE_SIZE / 2 / sizeof(void*); i < PAGE_SIZE / sizeof(void*); i++) {
            top[i] = current[i];
        }
        top = kernel_space_virt2physical(top);
    } else {
        for (size_t i = PAGE_SIZE / 2 / sizeof(void*); i < PAGE_SIZE / sizeof(void*); i++) {
            top[i] = (i - PAGE_SIZE / 2 / sizeof(void*)) << 28 | MMU_BIT_GLOBAL | MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_VALID;
        }
    }

    return top;
}

// mmu_map(mmu_level_1_t*, void*, void*, int) -> void
// Maps the given virtual address to the given physical address.
void mmu_map(mmu_level_1_t* top, void* virtual, void* physical, int flags) {
    intptr_t vpn2 = ((intptr_t) virtual >> 30) & 0x1ff;
    intptr_t vpn1 = ((intptr_t) virtual >> 21) & 0x1ff;
    intptr_t vpn0 = ((intptr_t) virtual >> 12) & 0x1ff;
    bool mmu_enabled = get_mmu() != NULL;

    if (mmu_enabled)
        top = kernel_space_phys2virtual(top);

    if (!(top[vpn2] & MMU_BIT_VALID)) {
        void* page = alloc_pages(1);
        top[vpn2] = ((intptr_t) page) >> 2 | MMU_BIT_VALID;
    }

    mmu_level_2_t* level2 = MMU_UNWRAP(2, top[vpn2]);
    if (mmu_enabled)
        level2 = kernel_space_phys2virtual(level2);

    if (!(level2[vpn1] & MMU_BIT_VALID)) {
        void* page = alloc_pages(1);
        level2[vpn1] = ((intptr_t) page) >> 2 | MMU_BIT_VALID;
    }

    mmu_level_3_t* level3 = MMU_UNWRAP(3, level2[vpn1]);
    if (mmu_enabled)
        level3 = kernel_space_phys2virtual(level3);

    if (!(level3[vpn0] & MMU_BIT_VALID)) {
        level3[vpn0] = ((intptr_t) physical & ~0x03) >> 2 | flags | MMU_BIT_VALID;
    }
}

// mmu_alloc(mmu_level_1_t*, void*, int) -> void*
// Allocates a new page and inserts it into the mmu table, returning the physical address.
void* mmu_alloc(mmu_level_1_t* top, void* virtual, int flags) {
    intptr_t vpn2 = ((intptr_t) virtual >> 30) & 0x1ff;
    intptr_t vpn1 = ((intptr_t) virtual >> 21) & 0x1ff;
    intptr_t vpn0 = ((intptr_t) virtual >> 12) & 0x1ff;
    bool mmu_enabled = get_mmu() != NULL;

    if (mmu_enabled)
        top = kernel_space_phys2virtual(top);
    if (!(top[vpn2] & MMU_BIT_VALID)) {
        void* page = alloc_pages(1);
        top[vpn2] = ((intptr_t) page) >> 2 | MMU_BIT_VALID;
    }

    mmu_level_2_t* level2 = MMU_UNWRAP(2, top[vpn2]);
    if (mmu_enabled)
        level2 = kernel_space_phys2virtual(level2);

    if (!(level2[vpn1] & MMU_BIT_VALID)) {
        void* page = alloc_pages(1);
        level2[vpn1] = ((intptr_t) page) >> 2 | MMU_BIT_VALID;
    }

    mmu_level_3_t* level3 = MMU_UNWRAP(3, level2[vpn1]);
    if (mmu_enabled)
        level3 = kernel_space_phys2virtual(level3);

    if (!(level3[vpn0] & MMU_BIT_VALID)) {
        void* physical = alloc_pages(1);
        level3[vpn0] = ((intptr_t) physical & ~0x03) >> 2 | flags | MMU_BIT_VALID;
        return physical;
    }

    return NULL;
}

// mmu_change_flags(mmu_level_1_t*, void*, int) -> void
// Changes the mmu page flags on the entry if the entry exists.
void mmu_change_flags(mmu_level_1_t* top, void* virtual, int flags) {
    intptr_t vpn2 = ((intptr_t) virtual >> 30) & 0x1ff;
    intptr_t vpn1 = ((intptr_t) virtual >> 21) & 0x1ff;
    intptr_t vpn0 = ((intptr_t) virtual >> 12) & 0x1ff;
    bool mmu_enabled = get_mmu() != NULL;

    if (mmu_enabled)
        top = kernel_space_phys2virtual(top);

    if (top[vpn2] & MMU_BIT_VALID) {
        mmu_level_2_t* level2 = MMU_UNWRAP(2, top[vpn2]);
        if (mmu_enabled)
            level2 = kernel_space_phys2virtual(level2);

        if (level2[vpn1] & MMU_BIT_VALID) {
            mmu_level_3_t* level3 = MMU_UNWRAP(3, level2[vpn1]);
            if (mmu_enabled)
                level3 = kernel_space_phys2virtual(level3);

            if (level3[vpn0] & MMU_BIT_VALID) {
                level3[vpn0] = (level3[vpn0] & ~0x3ff) | flags | MMU_BIT_VALID;
            }
        }
    }
}

// mmu_walk(mmu_level_1_t*, void*) -> intptr_t
// Walks the mmu page table to find the physical address for the corresponding virtual address.
intptr_t mmu_walk(mmu_level_1_t* top, void* virtual) {
    intptr_t vpn2 = ((intptr_t) virtual >> 30) & 0x1ff;
    intptr_t vpn1 = ((intptr_t) virtual >> 21) & 0x1ff;
    intptr_t vpn0 = ((intptr_t) virtual >> 12) & 0x1ff;
    bool mmu_enabled = get_mmu() != NULL;

    if (mmu_enabled)
        top = kernel_space_phys2virtual(top);

    if (MMU_UNWRAP(2, top[vpn2])) {
        mmu_level_2_t* level2 = MMU_UNWRAP(2, top[vpn2]);
        if (mmu_enabled)
            level2 = kernel_space_phys2virtual(level2);

        if (MMU_UNWRAP(3, level2[vpn1])) {
            mmu_level_3_t* level3 = MMU_UNWRAP(3, level2[vpn1]);
            if (mmu_enabled)
                level3 = kernel_space_phys2virtual(level3);
            return level3[vpn0];
        }
    }

    return 0;
}

// mmu_remove(mmu_level_1_t*, void*) -> void*
// Removes an entry from the mmu table.
void* mmu_remove(mmu_level_1_t* top, void* virtual) {
    intptr_t vpn2 = ((intptr_t) virtual >> 30) & 0x1ff;
    intptr_t vpn1 = ((intptr_t) virtual >> 21) & 0x1ff;
    intptr_t vpn0 = ((intptr_t) virtual >> 12) & 0x1ff;
    bool mmu_enabled = get_mmu() != NULL;

    if (mmu_enabled)
        top = kernel_space_phys2virtual(top);
    if (MMU_UNWRAP(2, top[vpn2])) {
        mmu_level_2_t* level2 = MMU_UNWRAP(2, top[vpn2]);
        if (mmu_enabled)
            level2 = kernel_space_phys2virtual(level2);

        if (MMU_UNWRAP(3, level2[vpn1])) {
            mmu_level_3_t* level3 = MMU_UNWRAP(3, level2[vpn1]);
            if (mmu_enabled)
                level3 = kernel_space_phys2virtual(level3);
            void* physical = MMU_UNWRAP(4, level3[vpn0]);
            level3[vpn0] = 0;
            return physical;
        }
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
    extern int sdata_end;
    extern int stack_start;
    extern int stack_top;
    extern int pages_bottom;
    extern page_t* heap_bottom;

    // Map kernel
    mmu_map_range_identity(top, &text_start, &data_start, MMU_BIT_READ | MMU_BIT_EXECUTE | MMU_BIT_GLOBAL);
    mmu_map_range_identity(top, &data_start, &ro_data_start, MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_GLOBAL);
    mmu_map_range_identity(top, &ro_data_start, &sdata_start, MMU_BIT_READ | MMU_BIT_GLOBAL);
    mmu_map_range_identity(top, &sdata_start, &sdata_end, MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_GLOBAL);
    mmu_map_range_identity(top, &stack_start, &stack_top, MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_GLOBAL);
    mmu_map_range_identity(top, &pages_bottom, heap_bottom, MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_GLOBAL);

    // Map fdt
    if (fdt != NULL)
        mmu_map_range_identity(top, fdt->header, ((void*) fdt->header) + be_to_le(32, fdt->header->totalsize), MMU_BIT_READ);

    // Map initrd
    if (initrd_start != initrd_end && initrd_start != NULL && initrd_end != NULL)
        mmu_map_range_identity(top, initrd_start, initrd_end, MMU_BIT_READ);
}

// remove_unused_entries(mmu_level_1_t*) -> void
// Removes unused entries from an mmu table.
void remove_unused_entries(mmu_level_1_t* top) {
    for (size_t i = 0; i < PAGE_SIZE / sizeof(void*); i++) {
        mmu_level_2_t* level2 = MMU_UNWRAP(2, top[i]);

        if (level2) {
            bool level2_used = false;
            for (size_t i = 0; i < PAGE_SIZE / sizeof(void*); i++) {
                mmu_level_3_t* level3 = MMU_UNWRAP(3, level2[i]);

                if (level3) {
                    bool level3_used = false;
                    for (size_t i = 0; i < PAGE_SIZE / sizeof(void*); i++) {
                        mmu_level_4_t* level4 = MMU_UNWRAP(4, level3[i]);
                        if (level4) {
                            level3_used = true;
                            level2_used = true;
                            break;
                        }
                    }

                    if (!level3_used) {
                        dealloc_pages(level3, 1);
                        level2[i] = 0;
                    }
                }
            }

            if (!level2_used) {
                dealloc_pages(level2, 1);
                top[i] = 0;
            }
        }
    }
}

// clean_mmu_table(mmu_level_1_t*) -> void
// Cleans up an mmu table.
void clean_mmu_table(mmu_level_1_t* top) {
    bool mmu_enabled = get_mmu() != NULL;
    if (mmu_enabled)
        top = kernel_space_phys2virtual(top);

    for (size_t i = 0; i < PAGE_SIZE / sizeof(void*); i++) {
        mmu_level_2_t* level2 = MMU_UNWRAP(2, top[i]);

        if (level2 && (top[i] & MMU_BIT_GLOBAL) == 0) {
            if (mmu_enabled)
                level2 = kernel_space_phys2virtual(level2);

            for (size_t i = 0; i < PAGE_SIZE / sizeof(void*); i++) {
                mmu_level_3_t* level3 = MMU_UNWRAP(3, level2[i]);

                if (level3 && (level2[i] & MMU_BIT_GLOBAL) == 0) {
                    if (mmu_enabled)
                        level3 = kernel_space_phys2virtual(level3);

                    for (size_t i = 0; i < PAGE_SIZE / sizeof(void*); i++) {
                        mmu_level_4_t* level4 = MMU_UNWRAP(4, level3[i]);
                        if (level4 && (level3[i] & MMU_BIT_GLOBAL) == 0)
                            dealloc_pages(level4, 1);
                    }

                    dealloc_pages(kernel_space_virt2physical(level3), 1);
                }
            }

            dealloc_pages(kernel_space_virt2physical(level2), 1);
        }
    }

    dealloc_pages(kernel_space_virt2physical(top), 1);
}
