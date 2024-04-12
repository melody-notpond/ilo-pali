#ifndef MMU_H
#define MMU_H
#include <stdbool.h>
#include <stdint.h>

#define MMU_BIT_VALID    0x01
#define MMU_BIT_READ     0x02
#define MMU_BIT_WRITE    0x04
#define MMU_BIT_EXEC     0x08
#define MMU_BIT_USER     0x10
#define MMU_BIT_GLOBAL   0x20
#define MMU_BIT_ACCESSED 0x40
#define MMU_BIT_DIRTY    0x80
#define MMU_BIT_RSV0     0x100
#define MMU_BIT_RSV1     0x200
#define MMU_ALL_BITS     0x3ff

struct mmu_root      { intptr_t data; };
struct __attribute__((packed)) mmu_entry { intptr_t data; };

static inline bool mmu_enabled() {
    intptr_t mmu;
    asm volatile("csrr %0, satp" : "=r" (mmu));
    return (mmu & 0x8000000000000000) != 0;
}


void *phys2safe(void *phys_addr);

void *phys2safe_nommu(void *phys_addr);

void *safe2phys(void *safe_addr);

void *safe2phys_nommu(void *safe_addr);

static inline bool mmu_root_equal(struct mmu_root mmu1, struct mmu_root mmu2) {
    return mmu1.data == mmu2.data;
}

static inline bool mmu_root_valid(struct mmu_root mmu) {
    return mmu.data != 0;
}

static inline bool mmu_entry_valid(struct mmu_entry entry) {
    return (entry.data & MMU_BIT_VALID) != 0;
}

static inline bool mmu_entry_read(struct mmu_entry entry) {
    return (entry.data & MMU_BIT_READ) != 0;
}

static inline bool mmu_entry_write(struct mmu_entry entry) {
    return (entry.data & MMU_BIT_WRITE) != 0;
}

static inline bool mmu_entry_exec(struct mmu_entry entry) {
    return (entry.data & MMU_BIT_EXEC) != 0;
}

static inline bool mmu_entry_frame(struct mmu_entry entry) {
    return mmu_entry_read(entry) || mmu_entry_write(entry) || mmu_entry_exec(entry);
}

static inline bool mmu_entry_user(struct mmu_entry entry) {
    return (entry.data & MMU_BIT_USER) != 0;
}

static inline bool mmu_entry_global(struct mmu_entry entry) {
    return (entry.data & MMU_BIT_GLOBAL) != 0;
}

static inline bool mmu_entry_accessed(struct mmu_entry entry) {
    return (entry.data & MMU_BIT_ACCESSED) != 0;
}

static inline bool mmu_entry_dirty(struct mmu_entry entry) {
    return (entry.data & MMU_BIT_DIRTY) != 0;
}

static inline int mmu_entry_flags(struct mmu_entry entry, int flags) {
    return entry.data & MMU_ALL_BITS & flags;
}

static inline void *mmu_entry_phys(struct mmu_entry entry) {
    return phys2safe((void *) ((entry.data & ~MMU_ALL_BITS) << 2));
}

// get_mmu() -> mmu_level_1_t*
// Gets the current value of satp and converts it into a pointer to the mmu table.
struct mmu_root get_mmu();

// set_mmu(mmu_level_1_t*) -> void
// Sets the satp csr to the provided mmu table pointer.
void set_mmu(struct mmu_root root);

// flush_mmu() -> void
// Flushes the table buffer or whatever its called.
inline void flush_mmu();

// create_mmu_table() -> mmu_level_1_t*
// Creates an empty mmu table.
struct mmu_root create_mmu_table();

struct mmu_entry *mmu_walk_to_entry(struct mmu_root root, void *virt_addr);

bool mmu_translate(struct mmu_root root, void *virt_addr, void **phys_addr);

// mmu_map(mmu_level_1_t*, void*, void*, int) -> void
// Maps the given virtual address to the given physical address.
int mmu_map(struct mmu_root root, void *virt_addr, void *physical, int flags);

// mmu_alloc(mmu_level_1_t*, void*, int) -> void*
// Allocates a new page and inserts it into the mmu table, returning the physical address.
void *mmu_alloc(struct mmu_root root, void *virt_addr, int flags);

// mmu_change_flags(mmu_level_1_t*, void*, int) -> void
// Changes the mmu page flags on the entry if the entry exists.
void mmu_change_flags(struct mmu_root root, void *virt_addr, int flags);

// mmu_remove(mmu_level_1_t*, void*) -> void*
// Removes an entry from the mmu table.
void *mmu_remove(struct mmu_root root, void *virt_addr);

// mmu_map_range_identity(mmu_level_1_t*, void*, void*, int) -> void
// Maps the given range to itself in the given mmu table.
void mmu_map_range_identity(
    struct mmu_root root,
    void *start,
    void *end,
    int flags
);

// clean_mmu_table(mmu_level_1_t*) -> void
// Cleans up an mmu table.
void clean_mmu_table(struct mmu_root root);

#endif /* MMU_H */
