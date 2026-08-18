ENTRY(_entry)

/* Flash-persistence symbols main.c references. Unused in a RAM test: _ptr_page points at an
 * erased flash page so bootloader_main reads 0xFFFFFFFF (>= flash limit) and goes straight to
 * command_processor instead of single_shot_boot. */
_ptr_page = 0x00054E00;
_bl_end   = 0x00000C00;

SECTIONS
{
    . = 0x20001800;
    .text : {
        *(.text.start)          /* _entry trampoline, byte 0 */
        . = ALIGN(128);         /* VTOR needs the IVT 128-byte aligned */
        _bootram_lma = .;
        _bootram_vma_start = .;
        _sram_ivt = .;
        KEEP(*(.sram_ivt))
        *(.text.micro)
        *(.text*)
        *(.rodata*)
        *(.data*)
        . = ALIGN(4);
        _bootram_vma_end = .;
    }

    .bss (NOLOAD) : { *(.bss*) *(COMMON) }

    /DISCARD/ : { *(.comment) *(.ARM.attributes) }
}
