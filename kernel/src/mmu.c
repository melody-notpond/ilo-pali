#include "console.h"
#include "memory.h"
#include "mmu.h"
#include <stdbool.h>

#define KERNEL_SPACE_OFFSET 0xffffffc000000000
#define MMU_PAGE_SIZE PAGE_SIZE
#define MMU_ENTRY_SIZE (sizeof(intptr_t))
#define MMU_ENTRY_COUNT (MMU_PAGE_SIZE / MMU_ENTRY_SIZE)
#define MMU_TOP_HALF (MMU_ENTRY_COUNT / 2)
#define VPN_COUNT 3

#define EMPTY_MMU(type) ((struct mmu_##type) { .data = 0, })
#define MMU_WRAP(type, value) ((struct mmu_##type) { .data = (value), })

void *phys2safe(void *phys_addr) {
    uintptr_t addr_int = (uintptr_t) phys_addr;
    if (mmu_enabled() && addr_int <= KERNEL_SPACE_OFFSET) {
        if (addr_int + KERNEL_SPACE_OFFSET >= KERNEL_SPACE_OFFSET)
            return phys_addr + KERNEL_SPACE_OFFSET;
        return NULL;
    }

    return phys_addr;
}

void *phys2safe_nommu(void *phys_addr) {
    uintptr_t addr_int = (uintptr_t) phys_addr;
    if (addr_int <= KERNEL_SPACE_OFFSET) {
        if (addr_int + KERNEL_SPACE_OFFSET >= KERNEL_SPACE_OFFSET)
            return phys_addr + KERNEL_SPACE_OFFSET;
        return NULL;
    }

    return phys_addr;
}

void *safe2phys(void *safe_addr) {
    uintptr_t addr_int = (uintptr_t) safe_addr;
    if (mmu_enabled() && addr_int >= KERNEL_SPACE_OFFSET)
        return safe_addr - KERNEL_SPACE_OFFSET;
    return safe_addr;
}

void *safe2phys_nommu(void *safe_addr) {
    uintptr_t addr_int = (uintptr_t) safe_addr;
    if (addr_int >= KERNEL_SPACE_OFFSET)
        return safe_addr - KERNEL_SPACE_OFFSET;
    return safe_addr;
}

void mmu_entry_set_flags(struct mmu_entry *entry, int flags) {
    if (!entry)
        return;
    intptr_t *entry_ptr = (intptr_t *) entry;
    *entry_ptr = (*entry_ptr & ~MMU_ALL_BITS) | (flags & MMU_ALL_BITS);
}

void mmu_entry_set_phys(struct mmu_entry *entry, void *phys) {
    if (!entry)
        return;
    intptr_t *entry_ptr = (intptr_t *) entry;
    intptr_t phys_int = (intptr_t) phys;
    *entry_ptr = (*entry_ptr & MMU_ALL_BITS) | ((phys_int & ~0xfff) >> 2);
}

void get_vpns(void *addr, intptr_t vpns[3], intptr_t *offset) {
    vpns[0] = (((intptr_t) addr) >> 30) & 0x1ff;
    vpns[1] = (((intptr_t) addr) >> 21) & 0x1ff;
    vpns[2] = (((intptr_t) addr) >> 12) & 0x1ff;

    if (offset)
        *offset = ((intptr_t) addr) & 0xfff;
}

// get_mmu() -> mmu_level_1_t*
// Gets the current value of satp and converts it into a pointer to the mmu table.
struct mmu_root get_mmu() {
    intptr_t mmu;
    asm volatile("csrr %0, satp" : "=r" (mmu));

    if ((mmu & 0x8000000000000000) != 0)
        return MMU_WRAP(root, (mmu & 0x00000fffffffffff) << 12);
    return EMPTY_MMU(root);
}

struct mmu_entry *mmu_root_get_any(struct mmu_root root, size_t i) {
    if (!mmu_root_valid(root))
        return NULL;
    if (i >= MMU_ENTRY_COUNT)
        return NULL;
    return &((struct mmu_entry *) phys2safe((void *) root.data))[i];
}

struct mmu_entry *mmu_root_get(struct mmu_root root, size_t i) {
    struct mmu_entry *entry = mmu_root_get_any(root, i);
    if (entry && mmu_entry_valid(*entry))
        return entry;
    return NULL;
}

struct mmu_entry *mmu_entry_get_any(struct mmu_entry entry, size_t i) {
    if (!mmu_entry_valid(entry) || mmu_entry_frame(entry))
        return NULL;
    if (i >= MMU_ENTRY_COUNT)
        return NULL;
    void *page_addr = mmu_entry_phys(entry);
    if (!page_addr)
        return NULL;
    return &((struct mmu_entry *) phys2safe(mmu_entry_phys(entry)))[i];
}

struct mmu_entry *mmu_entry_get(struct mmu_entry entry, size_t i) {
    struct mmu_entry *next_entry = mmu_entry_get_any(entry, i);
    if (next_entry && mmu_entry_valid(*next_entry))
        return next_entry;
    return NULL;
}

// set_mmu(mmu_level_1_t*) -> void
// Sets the satp csr to the provided mmu table pointer.
void set_mmu(struct mmu_root root) {
    if (mmu_root_valid(root)) {
        struct mmu_root current = get_mmu();

        if (root.data == current.data)
            return;

        // TODO: i dont think this is necessary
        // if (mmu_root_valid(current)) {
        //     top = kernel_space_phys2virtual(top);
        //     current = kernel_space_phys2virtual(current);

        //     for (size_t i = PAGE_SIZE / 2 / sizeof(void*); i < PAGE_SIZE / sizeof(void*); i++) {
        //         top[i] = current[i];
        //     }

        //     top = kernel_space_virt2physical(top);
        // }

        uint64_t mmu = 0x8000000000000000 | (root.data >> 12);
        asm volatile("csrw satp, %0" : "=r" (mmu));
        asm volatile("sfence.vma");
    }
}

// flush_mmu() -> void
// Flushes the table buffer or whatever its called.
inline void flush_mmu() {
    asm volatile("sfence.vma");
}


struct mmu_entry *mmu_walk_to_entry(struct mmu_root root, void *virt_addr) {
    intptr_t vpns[VPN_COUNT];
    get_vpns(virt_addr, vpns, NULL);

    struct mmu_entry *entry = mmu_root_get(root, vpns[0]);
    if (!entry || mmu_entry_frame(*entry))
        return entry;

    for (int i = 1; i < VPN_COUNT; i++) {
        entry = mmu_entry_get(*entry, vpns[i]);
        if (!entry || mmu_entry_frame(*entry))
            return entry;
    }

    return entry;
}

bool mmu_translate(struct mmu_root root, void *virt_addr, void **phys_addr) {
    intptr_t vpns[VPN_COUNT];
    intptr_t offset;
    get_vpns(virt_addr, vpns, &offset);

    struct mmu_entry *entry = mmu_root_get(root, vpns[0]);
    if (!entry)
        return false;

    for (int i = 1; i < VPN_COUNT; i++) {
        if (mmu_entry_frame(*entry)) {
            for (int j = VPN_COUNT - 1; j >= i; j--) {
                offset |= vpns[j] << (12 + (VPN_COUNT - j - 1) * 9);
            }

            *phys_addr = (void *)
                (((intptr_t) mmu_entry_phys(*entry)) | offset);
            return true;
        }

        entry = mmu_entry_get(*entry, vpns[i]);
        if (!entry)
            return false;
    }

    if (!mmu_entry_frame(*entry))
        return false;
    *phys_addr = (void *) (((intptr_t) mmu_entry_phys(*entry)) | offset);
    return true;
}

// mmu_map(mmu_level_1_t*, void*, void*, int) -> void
// Maps the given virtual address to the given physical address.
int mmu_map(struct mmu_root root, void *virt_addr, void *physical, int flags) {
    intptr_t vpns[VPN_COUNT];
    get_vpns(virt_addr, vpns, NULL);

    struct mmu_entry *entry = mmu_root_get_any(root, vpns[0]);
    for (int i = 1; i < VPN_COUNT; i++) {
        if (!entry || (mmu_entry_valid(*entry) && mmu_entry_frame(*entry)))
            return -1;
        if (!mmu_entry_valid(*entry)) {
            void *page = alloc_pages(1);
            mmu_entry_set_flags(entry,
                MMU_BIT_VALID);
            mmu_entry_set_phys(entry, page);
        }

        entry = mmu_entry_get_any(*entry, vpns[i]);
    }

    if (!entry || mmu_entry_valid(*entry))
        return -1;
    mmu_entry_set_flags(entry, flags
        | MMU_BIT_VALID | MMU_BIT_ACCESSED | MMU_BIT_DIRTY);
    mmu_entry_set_phys(entry, physical);
    return 0;
}

// mmu_alloc(mmu_level_1_t*, void*, int) -> void*
// Allocates a new page and inserts it into the mmu table, returning the physical address.
void *mmu_alloc(struct mmu_root root, void *virt_addr, int flags) {
    void *physical = alloc_pages(1);
    if (mmu_map(root, virt_addr, physical, flags)) {
        dealloc_pages(physical, 1);
        return NULL;
    }

    return physical;
}

// mmu_change_flags(mmu_level_1_t*, void*, int) -> void
// Changes the mmu page flags on the entry if the entry exists.
void mmu_change_flags(struct mmu_root root, void *virt_addr, int flags) {
    struct mmu_entry *entry = mmu_walk_to_entry(root, virt_addr);
    if (entry)
        mmu_entry_set_flags(entry, flags
            | MMU_BIT_VALID | MMU_BIT_ACCESSED | MMU_BIT_DIRTY);
}

// mmu_remove(mmu_level_1_t*, void*) -> void*
// Removes an entry from the mmu table.
void *mmu_remove(struct mmu_root root, void *virt_addr) {
    struct mmu_entry *entry = mmu_walk_to_entry(root, virt_addr);
    if (!entry)
        return NULL;
    void *physical = mmu_entry_phys(*entry);
    mmu_entry_set_flags(entry, 0);
    return physical;
}

// mmu_map_range_identity(mmu_level_1_t*, void*, void*, int) -> void
// Maps the given range to itself in the given mmu table.
void mmu_map_range_identity(
    struct mmu_root root,
    void *start,
    void *end,
    int flags
) {
    for (void* p = start; p < end; p += PAGE_SIZE) {
        mmu_map(root, p, p, flags);
    }
}

// identity_map_kernel(fdt_t*, void*, void*) -> void
// Identity maps the kernel in the given mmu table.
void identity_map_kernel(struct mmu_root root) {
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
    mmu_map_range_identity(root, &text_start, &data_start,
        MMU_BIT_READ | MMU_BIT_EXEC | MMU_BIT_GLOBAL);
    mmu_map_range_identity(root, &data_start, &ro_data_start,
        MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_GLOBAL);
    mmu_map_range_identity(root, &ro_data_start, &sdata_start,
        MMU_BIT_READ | MMU_BIT_GLOBAL);
    mmu_map_range_identity(root, &sdata_start, &sdata_end,
        MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_GLOBAL);
    mmu_map_range_identity(root, &stack_start, &stack_top,
        MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_GLOBAL);
    mmu_map_range_identity(root, &pages_bottom, heap_bottom,
        MMU_BIT_READ | MMU_BIT_WRITE | MMU_BIT_GLOBAL);
}

// create_mmu_table() -> mmu_level_1_t*
// Creates an empty mmu table.
struct mmu_root create_mmu_table() {
    intptr_t* top = phys2safe(alloc_pages(1));

    for (size_t i = MMU_TOP_HALF; i < MMU_ENTRY_COUNT; i++) {
        intptr_t page = ((i - MMU_TOP_HALF) << 28)
            | MMU_BIT_GLOBAL
            | MMU_BIT_READ
            | MMU_BIT_WRITE
            | MMU_BIT_VALID;
        top[i] = page;
    }

    struct mmu_root root = MMU_WRAP(root, (intptr_t) safe2phys(top));
    identity_map_kernel(root);
    return root;
}

// clean_mmu_table(mmu_level_1_t*) -> void
// Cleans up an mmu table.
void clean_mmu_table(struct mmu_root root) {
    // TODO
    (void) root;
}
