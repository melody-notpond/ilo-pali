#ifndef ELF_H
#define ELF_H

#include <stddef.h>
#include <stdint.h>

typedef struct __attribute__((packed)) {
    unsigned char ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t program_header_offset;
    uint32_t section_header_offset;
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
    uint32_t flags;
    uint32_t addr;
    uint32_t offset;
    uint32_t size;
    uint32_t link;
    uint32_t info;
    uint32_t addr_align;
    uint32_t entry_size;
} elf_section_header_t;

typedef struct {
	uint32_t type;
	uint32_t offset;
	uint32_t virtual_addr;
	uint32_t physical_addr;
	uint32_t file_size;
	uint32_t memory_size;
	uint32_t flags;
	uint32_t align;
} elf_program_header_t;

typedef struct {
    elf_header_t* header;
    elf_section_header_t* section_headers;
    elf_program_header_t* program_headers;
    char* string_table;

    size_t size;
} elf_t;

// verify_elf(void* data, size_t size) -> elf_t
// Verifies an elf file.
elf_t verify_elf(void* data, size_t size);

#endif /* ELF_H */
