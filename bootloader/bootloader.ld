ENTRY(Reset_Handler)
SECTIONS
{
    . = 0x00000000;
    .isr_vector : { KEEP(*(.isr_vector)) }
    .text.micro : SUBALIGN(4) { *(.text.micro) }

    _bootram_lma = .;

    . = 0x20000000;
    .bootram : AT (_bootram_lma)
    {
        _bootram_vma_start = .;
        _sram_ivt = .;
        KEEP(*(.sram_ivt))
        *(.text*)
        *(.rodata*)
        *(.data*)
        . = ALIGN(4);
        _bootram_vma_end = .;
    }

    _ptr_page = (_bootram_lma + (_bootram_vma_end - _bootram_vma_start) + 0x1FF) & ~0x1FF;
    _bl_end = _ptr_page + 0x200;

    /* App payload lives at flash 0x8000 (link/flash_after_bl.ld). Fail the build if the
     * bootloader ever grows into it -- bump both 0x8000 values together if so. */
    ASSERT(_bl_end <= 0x8000, "bootloader overran the 0x8000 app base -- raise APP_BASE")

    .bss (NOLOAD) : { *(.bss*) *(COMMON) }

    /DISCARD/ : { *(.comment) *(.ARM.attributes) }
}
