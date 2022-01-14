#include "console.h"
#include "elf.h"

// verify_elf(void* data, size_t size) -> elf_t
// Verifies an elf file.
elf_t verify_elf(void* data, size_t size) {
    if (data == NULL)
        return (elf_t) { 0 };

    elf_header_t* header = data;

    if (header->ident[0] != 0x7f || header->ident[1] != 'E' || header->ident[2] != 'L' || header->ident[3] != 'F') {
        console_printf("data located at %p is not an elf file\n", data);
        return (elf_t) { 0 };
    }


    if (header->ident[4] != 2) {
        console_printf("unknown elf class 0x%x\n", header->ident[4]);
        return (elf_t) { 0 };
    }

    if (header->ident[5] != 1) {
        console_printf("unknown data encoding 0x%x\n", header->ident[5]);
        return (elf_t) { 0 };
    }

    if (header->version != 1) {
        console_printf("invalid version 0x%x\n", header->version);
        return (elf_t) { 0 };
    }

    if (header->machine != 243) {
        console_printf("invalid machine type %x\n", header->ident[5]);
        return (elf_t) { 0 };
    }

    // TODO: float abi stuff with header->type

    elf_t elf = {
        .header = header,
        .size = size,
    };
    elf.string_table = (char*) data + get_elf_section_header(&elf, elf.header->section_header_string_index)->offset;
    return elf;
}

// get_elf_section_header(elf_t*, size_t) -> elf_section_header_t*
// Returns the section header at the provided index.
elf_section_header_t* get_elf_section_header(elf_t* elf, size_t i) {
    return ((void*) elf->header) + elf->header->section_header_offset + elf->header->section_header_entry_size * i;
}

// get_elf_program_header(elf_t*, size_t) -> elf_section_header_t*
// Returns the section header at the provided index.
elf_program_header_t* get_elf_program_header(elf_t* elf, size_t i) {
    return ((void*) elf->header) + elf->header->program_header_offset + elf->header->program_header_entry_size * i;
}
