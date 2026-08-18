#include "thc_core.h"

extern uint32_t _bootram_lma;
extern uint32_t _bootram_vma_start;
extern uint32_t _bootram_vma_end;
extern uint32_t _sram_ivt;
extern void bootloader_main(void);

__attribute__((section(".text.micro"), used))
void micro_reset(void)
{
    const uint32_t *src = &_bootram_lma;
    uint32_t *dst = &_bootram_vma_start;
    while (dst < &_bootram_vma_end)
        *dst++ = *src++;

    THC_SCB->VTOR = (uint32_t)&_sram_ivt;
    __asm__ volatile("dsb\nisb" ::: "memory");

    bootloader_main();
    for (;;) { }
}
