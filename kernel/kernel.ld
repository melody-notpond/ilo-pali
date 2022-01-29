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
    PROVIDE( __global_pointer$ = . + (4K / 2) );
    PROVIDE(sdata_start = .);
    .sdata : { *(.sdata) }
    .sdata : { *(.bss) }
    .sdata : { *(.sbss) }
    .debug : { *(.debug) }
    PROVIDE(sdata_end = .);

    . += 0x1000;

    . = ALIGN(4096);
    PROVIDE(stack_start = .);
    . += 0x8000;
    PROVIDE(stack_top = .);

    . += 0x1000;

    /* Pages */
    . = ALIGN(4096);
    PROVIDE(pages_bottom = .);

    /* End of uninitalized data segement */
}


