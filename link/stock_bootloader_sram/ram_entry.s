/* Implant entry for the SRAM bootloader. The 0x1554 RAM harness jumps to 0x20001801,
 * so byte 0 of the image must be executable. Hardware never ran a reset for us, so unlike
 * the flash boot path there's no vector[0]->MSP load: we set SP ourselves, then hand off
 * to micro_reset (identity LMA==VMA copy is a harmless no-op, then it sets VTOR + calls
 * bootloader_main). Nothing else in the flash boot path changes.
 */
.syntax unified
.thumb
.cpu cortex-m0

.section .text.start
.thumb_func
.global _entry
_entry:
    ldr r0, =0x20002800      /* BL_MSP, same stack top as the flash build */
    mov sp, r0
    bl  micro_reset          /* never returns */
