#ifndef MMU_H
#define MMU_H

#include <stddef.h>
#include <stdint.h>

#include "fdt.h"

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

// create_mmu_table() -> mmu_level_1_t*
// Creates an empty mmu table.
mmu_level_1_t* create_mmu_table();

// mmu_map(mmu_level_1_t*, void*, void*, int) -> void
// Maps the given virtual address to the given physical address.
void mmu_map(mmu_level_1_t* top, void* virtual_, void* physical, int flags);

// mmu_alloc(mmu_level_1_t*, void*, int) -> void*
// Allocates a new page and inserts it into the mmu table, returning the physical address.
void* mmu_alloc(mmu_level_1_t* top, void* virtual_, int flags);

// identity_map_kernel(fdt_t*, void*, void*) -> void
// Identity maps the kernel in the given mmu table.
void identity_map_kernel(mmu_level_1_t* top, fdt_t* fdt, void* initrd_start, void* initrd_end);

#endif /* MMU_H */
