#ifndef ELF_H
#define ELF_H

#include <stddef.h>
#include <stdint.h>

#define ELF_EXECUTABLE 2

typedef struct __attribute__((packed)) {
    unsigned char ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t program_header_offset;
    uint64_t section_header_offset;
    uint32_t flags;
    uint16_t elf_header_size;
    uint16_t program_header_entry_size;
    uint16_t program_header_num;
    uint16_t section_header_entry_size;
    uint16_t section_header_num;
    uint16_t section_header_string_index;
} elf_header_t;

typedef struct __attribute__((packed)) {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t addr;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t addr_align;
    uint64_t entry_size;
} elf_section_header_t;

typedef struct __attribute__((packed)) {
	uint32_t type;
	uint32_t flags;
	uint64_t offset;
	uint64_t virtual_addr;
	uint64_t physical_addr;
	uint64_t file_size;
	uint64_t memory_size;
	uint64_t align;
} elf_program_header_t;

typedef struct {
    elf_header_t* header;
    char* string_table;
    size_t size;
} elf_t;

// verify_elf(void* data, size_t size) -> elf_t
// Verifies an elf file.
elf_t verify_elf(void* data, size_t size);

// get_elf_section_header(elf_t*, size_t) -> elf_section_header_t*
// Returns the section header at the provided index.
elf_section_header_t* get_elf_section_header(elf_t* elf, size_t i);

// get_elf_program_header(elf_t*, size_t) -> elf_section_header_t*
// Returns the section header at the provided index.
elf_program_header_t* get_elf_program_header(elf_t* elf, size_t i);

#endif /* ELF_H */
