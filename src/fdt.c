#include "fdt.h"
#include "console.h"
#include "string.h"

// be_to_le(unsigned long long, void*) -> unsigned long long
// Converts a big endian number into a little endian number.
unsigned long long be_to_le(unsigned long long size, void* be) {
    unsigned char* be_char = be;
    unsigned long long byte_count = size / 8;
    unsigned long long result = 0;

    for (unsigned long long i = 0; i < byte_count; i++) {
        result |= ((unsigned long long) be_char[i]) << ((byte_count - i - 1) * 8);
    }

    return result;
}

// verify_fdt(void*) -> fdt_t
// Verifies a fdt by checking its magic number.
fdt_t verify_fdt(void* fdt) {
    fdt_header_t* header = fdt;
    if (be_to_le(32, header->magic) == 0xd00dfeed && be_to_le(32, header->version) == 17)
        return (fdt_t) {
            .header = header,
            .memory_reservation_block = fdt + be_to_le(32, header->off_mem_rsvmap),
            .structure_block = fdt + be_to_le(32, header->off_dt_struct),
            .strings_block = fdt + be_to_le(32, header->off_dt_strings)
        };

    return (fdt_t) { 0 };
}

// dump_fdt(fdt_t*, void*) -> void
// Dumps an fdt to the UART.
void dump_fdt(fdt_t* fdt, void* node) {
    if (fdt->header == (void*) 0) {
        console_puts("Invalid flat device tree\n");
        return;
    }

    if (node == (void*) 0) {
        console_printf(
            "Header at %p:\n"
            "    magic: 0x%llx\n"
            "    total size: 0x%llx\n"
            "    structure offset: 0x%llx\n"
            "    strings offset: 0x%llx\n"
            "    memory reserved map offset: 0x%llx\n"
            "    version: 0x%llx\n"
            "    last compatible version: 0x%llx\n"
            "    boot cpu id: 0x%llx\n"
            "    strings size: 0x%llx\n"
            "    structure size: 0x%llx\n"
            , fdt->header
            , be_to_le(32, fdt->header->magic)
            , be_to_le(32, fdt->header->totalsize)
            , be_to_le(32, fdt->header->off_dt_struct)
            , be_to_le(32, fdt->header->off_dt_strings)
            , be_to_le(32, fdt->header->off_mem_rsvmap)
            , be_to_le(32, fdt->header->version)
            , be_to_le(32, fdt->header->last_comp_version)
            , be_to_le(32, fdt->header->boot_cpuid_phys)
            , be_to_le(32, fdt->header->size_dt_strings)
            , be_to_le(32, fdt->header->size_dt_struct)
        );

        struct fdt_reserve_entry* entry = fdt->memory_reservation_block;
        unsigned long long addr = be_to_le(64, entry->address);
        unsigned long long size = be_to_le(64, entry->size);
        console_puts("Memory reserved map:\n");
        while (addr || size) {
            console_printf("    reserved %llx-%llx (%llx bytes)\n", addr, addr + size, size);
            entry++;
            addr = be_to_le(64, entry->address);
            size = be_to_le(64, entry->size);
        }
        console_puts("End of memory reserved map\nroot:\n");
    }

    void* ptr = node != (void*) 0 ? node : fdt->structure_block;
    int indent = 0;
    unsigned long long current = be_to_le(32, ptr);
    do {
        switch ((fdt_node_type_t) current) {
            case FDT_BEGIN_NODE: {
                for (int i = 0; i < indent; i++) {
                    console_puts("    ");
                }

                indent += 1;
                ptr += 4;
                char* c = ptr;
                if (node || indent != 1) {
                    while (*c) {
                        console_printf("%c", *c++);
                    }
                    console_puts(":\n");
                }

                ptr = (void*) ((unsigned long long) (c + 4) & ~0x3);
                break;
            }

            case FDT_END_NODE:
                indent -= 1;
                ptr += 4;
                break;

            case FDT_PROP:
                for (int i = 0; i < indent; i++) {
                    console_puts("    ");
                }

                ptr += 4;
                unsigned int len = be_to_le(32, ptr);
                ptr += 4;
                unsigned int name_offset = be_to_le(32, ptr);
                ptr += 4;

                console_printf("property %s = ", fdt->strings_block + name_offset);

                if (*((char*) ptr) == 0) {
                    console_printf("0x%llx (0x%x bytes)\n", be_to_le(len * 8, ptr), len);
                } else {
                    for (unsigned int i = 0; i < len; i++) {
                        console_printf("%c", ((char*) ptr)[i]);
                    }
                    console_printf(" (0x%x bytes)\n", len);
                }

                ptr = (void*) ((unsigned long long) (ptr + len + 3) & ~0x3);
                break;

            case FDT_NOP:
                ptr += 4;
                break;

            case FDT_END:
                break;
        }
    } while ((current = be_to_le(32, ptr)) != FDT_END && indent > 0);
}

// fdt_find(fdt_t*, char*, void*) -> void*
// Finds a device tree node with the given name. Returns null on failure.
void* fdt_find(fdt_t* fdt, char* name, void* last) {
    if (last == (void*) 0) {
        last = fdt->structure_block;
    } else {
        last += 4;
        char* c = last;
        while (*c++);
        last = (void*) ((unsigned long long) (c + 4) & ~0x3);
    }

    unsigned long long current;
    while ((current = be_to_le(32, last)) != FDT_END) {
        switch ((fdt_node_type_t) current) {
            case FDT_BEGIN_NODE: {
                char* c = last + 4;
                char* temp_name = name;

                while (*c != '\0' && *c != '@' && *temp_name) {
                    if (*c != *temp_name)
                        break;
                    c++;
                    temp_name++;
                }

                if ((*c == '\0' || *c == '@') && *temp_name == '\0')
                    return last;

                while (*c++);

                last = (void*) ((unsigned long long) (c + 3) & ~0x3);
                break;
            }

            case FDT_END_NODE:
                last += 4;
                break;

            case FDT_PROP:
                last += 4;
                unsigned int len = be_to_le(32, last);
                last += 4;
                //unsigned int name_offset = be_to_le(32, last);
                last += 4;
                last = (void*) ((unsigned long long) (last + len + 3) & ~0x3);
                break;

            case FDT_NOP:
                last += 4;
                break;

            case FDT_END:
                break;
        }
    }

    return (void*) 0;
}

// fdt_path(fdt_t*, char*, void*) -> void*
// Finds a device tree node with the given path. Returns null on failure.
void* fdt_path(fdt_t* fdt, char* path, void* last) {
    if (last == (void*) 0 || path[0] == '/') {
        last = fdt->structure_block;
        path += path[0] == '/';
    } else {
        last += 4;
        char* c = last;
        while (*c++);
        last = (void*) ((unsigned long long) (c + 4) & ~0x3);
    }

    unsigned long long current;
    unsigned long long depth = 1;
    unsigned long long intended_depth = 1;
    while ((current = be_to_le(32, last)) != FDT_END) {
        switch ((fdt_node_type_t) current) {
            case FDT_BEGIN_NODE: {
                char* c = last + 4;
                char* temp_name = path;

                while (*c && *temp_name && *temp_name != '/') {
                    if (*c != *temp_name)
                        break;
                    c++;
                    temp_name++;
                }

                if (*c == '\0' && *temp_name == '\0')
                    return last;
                else if (*c == '\0' && *temp_name == '/') {
                    intended_depth++;
                    path = temp_name + 1;
                }
                depth++;

                while (*c++);

                last = (void*) ((unsigned long long) (c + 3) & ~0x3);
                break;
            }

            case FDT_END_NODE:
                last += 4;
                depth--;
                if (depth < intended_depth) {
                    return (void*) 0;
                }
                break;

            case FDT_PROP:
                last += 4;
                unsigned int len = be_to_le(32, last);
                last += 4;
                //unsigned int name_offset = be_to_le(32, last);
                last += 4;
                last = (void*) ((unsigned long long) (last + len + 3) & ~0x3);
                break;

            case FDT_NOP:
                last += 4;
                break;

            case FDT_END:
                break;
        }
    }

    return (void*) 0;
}

// fdt_get_node_addr(void*) -> unsigned long long
// Gets the address after the @ sign in a device tree node.
unsigned long long fdt_get_node_addr(void* node) {
    if (be_to_le(32, node) != FDT_BEGIN_NODE) {
        return 0;
    }

    char* c = node + 4;
    while (*c && *c++ != '@');
    if (*c == '\0') {
        return 0;
    }

    unsigned long long result = 0;
    while (*c) {
        result <<= 4;
        unsigned long long v = (unsigned long long) *c++;
        switch (v) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                v = v - '0';
                break;
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
                v = v - 'a' + 0xa;
                break;
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
                v = v - 'A' + 0xA;
                break;
        }

        result |= v;
    }

    return result;
}

// fdt_get_property(fdt_t*, void*, char*) -> struct fdt_property
// Gets a property from a device tree node.
struct fdt_property fdt_get_property(fdt_t* fdt, void* node, char* key) {
    if (node == (void*) 0) {
        node = fdt->structure_block;
    }

    if (be_to_le(32, node) != FDT_BEGIN_NODE) {
        return (struct fdt_property) { 0 };
    }

    unsigned long long depth = 0;
    unsigned long long current;
    while ((current = be_to_le(32, node)) != FDT_END) {
        switch ((fdt_node_type_t) current) {
            case FDT_BEGIN_NODE: {
                depth++;
                char* c = node + 4;

                while (*c++);

                node = (void*) ((unsigned long long) (c + 3) & ~0x3);
                break;
            }

            case FDT_END_NODE:
                depth--;
                if (depth == 0)
                    return (struct fdt_property) { 0 };
                node += 4;
                break;

            case FDT_PROP:
                node += 4;
                unsigned int len = be_to_le(32, node);
                node += 4;
                unsigned int name_offset = be_to_le(32, node);
                node += 4;

                if (depth == 1 && !strcmp(fdt->strings_block + name_offset, key)) {
                    return (struct fdt_property) {
                        .len = len,
                        .key = fdt->strings_block + name_offset,
                        .data = node
                    };
                }

                node = (void*) ((unsigned long long) (node + len + 3) & ~0x3);
                break;

            case FDT_NOP:
                node += 4;
                break;

            case FDT_END:
                break;
        }
    }

    return (struct fdt_property) { 0 };
}

