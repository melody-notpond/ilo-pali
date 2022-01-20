#ifndef MMU_H
#define MMU_H

#include <stddef.h>
#include <stdint.h>

#include "fdt.h"

#define MMU_UNWRAP(t, v) ((mmu_level_##t##_t*) (((v) << 2) & ~0xfff))

#define MMU_BIT_VALID       0x01
#define MMU_BIT_READ        0x02
#define MMU_BIT_WRITE       0x04
#define MMU_BIT_EXECUTE     0x08
#define MMU_BIT_USER        0x10
#define MMU_BIT_GLOBAL      0x20
#define MMU_BIT_ACCESSED    0x40
#define MMU_BIT_DIRTY       0x80

typedef intptr_t mmu_level_1_t;
typedef intptr_t mmu_level_2_t;
typedef intptr_t mmu_level_3_t;
typedef intptr_t mmu_level_4_t;

// get_mmu() -> mmu_level_1_t*
// Gets the current value of satp and converts it into a pointer to the mmu table.
mmu_level_1_t* get_mmu();

// set_mmu(mmu_level_1_t*) -> void
// Sets the satp csr to the provided mmu table pointer.
void set_mmu(mmu_level_1_t* top);

// flush_mmu() -> void
// Flushes the table buffer or whatever its called.
void flush_mmu();

// create_mmu_table() -> mmu_level_1_t*
// Creates an empty mmu table.
mmu_level_1_t* create_mmu_table();

// mmu_premap(mmu_level_1_t*, void*) -> void
// Premaps the given address in the given mmu table.
void mmu_premap(mmu_level_1_t* top, void* virtual_);

// mmu_map(mmu_level_1_t*, void*, void*, int) -> void
// Maps the given virtual address to the given physical address.
void mmu_map(mmu_level_1_t* top, void* virtual_, void* physical, int flags);

// mmu_alloc(mmu_level_1_t*, void*, int) -> void*
// Allocates a new page and inserts it into the mmu table, returning the physical address.
void* mmu_alloc(mmu_level_1_t* top, void* virtual_, int flags);

// mmu_change_flags(mmu_level_1_t*, void*, int) -> void
// Changes the mmu page flags on the entry if the entry exists.
void mmu_change_flags(mmu_level_1_t* top, void* virtual_, int flags);

// mmu_walk(mmu_level_1_t*, void*) -> intptr_t
// Walks the mmu page table to find the physical address for the corresponding virtual address.
intptr_t mmu_walk(mmu_level_1_t* top, void* virtual_);

// mmu_remove(mmu_level_1_t*, void*) -> void*
// Removes an entry from the mmu table.
void* mmu_remove(mmu_level_1_t* top, void* virtual_);

// mmu_kalloc(mmu_level_1_t*, void*, size_t)
// Allocates pages in the top quarter of the mmu table.
void* mmu_kalloc(mmu_level_1_t* top, void* physical, size_t count);

// identity_map_kernel(fdt_t*, void*, void*) -> void
// Identity maps the kernel in the given mmu table.
void identity_map_kernel(mmu_level_1_t* top, fdt_t* fdt, void* initrd_start, void* initrd_end);

// remove_unused_entries(mmu_level_1_t*) -> void
// Removes unused entries from an mmu table.
void remove_unused_entries(mmu_level_1_t* top);

// mmu_map_mmu(mmu_level_1_t*, mmu_level_1_t*) -> void
// Maps an mmu into an mmu. (more mmu-ception :florshed:)
void mmu_map_mmu(mmu_level_1_t* top, mmu_level_1_t* entry);

// remove_mmu_from_mmu(mmu_level_1_t*, mmu_level_1_t*) -> void
// Removes an mmu table from an mmu table. (mmu-ception moment)
void remove_mmu_from_mmu(mmu_level_1_t* top, mmu_level_1_t* entry);

// clean_mmu_table(mmu_level_1_t*) -> void
// Cleans up an mmu table.
void clean_mmu_table(mmu_level_1_t* top);

#endif /* MMU_H */
