ENTRY(_start)
SECTIONS
{
    . = 0x8000;
    .isr_vector : { KEEP(*(.isr_vector)) }
    .text : { *(.text.boot) *(.text*) }
    .rodata : {
        *(.rodata*)
        . = ALIGN(512);
    } = 0xFFFFFFFF
    _sidata = .;

    . = 0x20001800;
    .data : AT (_sidata) { _sdata = .; *(.data*) _edata = .; }
    .bss (NOLOAD) : { _sbss = .; *(.bss*) *(COMMON) _ebss = .; }

    ASSERT(SIZEOF(.data) == 0,
        "app carries .data: its flash LMA lands past the 512-pad -- pad after .data before flashing")

    /DISCARD/ : { *(.comment) *(.ARM.attributes) }
}
