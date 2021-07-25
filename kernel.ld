OUTPUT_ARCH( "riscv" )
OUTPUT_FORMAT( "elf64-littleriscv" )
ENTRY( _start )
SECTIONS
{
    /* text: text code section */
    . = 0x80200000;
    PROVIDE(text_start = .);
    .text : { *(.text) }

    /* data: Initialized data segment */
    .gnu_build_id : { *(.note.gnu.build-id) }

    . = ALIGN(4096);
    PROVIDE(data_start = .);
    .data : { *(.data) }

    . = ALIGN(4096);
    PROVIDE(ro_data_start = .);
    .rodata : { *(.rodata) }

    . = ALIGN(4096);
    PROVIDE(sdata_start = .);
    .sdata : { *(.sdata) }
    .debug : { *(.debug) }

    . = ALIGN(4096);
    PROVIDE(stack_start = .);
    . += 0x4000;
    PROVIDE(stack_top = .);

    /* Heap */
    . = ALIGN(4096);
    PROVIDE(heap_bottom = .);
    . += 0x80000;

    /* Pages */
    . = ALIGN(4096);
    PROVIDE(pages_bottom = .);

    /* End of uninitalized data segement */
}


