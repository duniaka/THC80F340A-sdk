#include "thc80f340a.h"

void _start(void);

/* .ram_state (0x20001800) home for the relocated table. 48 words = 16 ARMv6-M exception vectors +
 * up to 32 IRQ lines. aligned(128): VTOR honors bits[29:7] only. */
static uint32_t reloc_ivt[48] __attribute__((aligned(128)));

void _start(void)
{
    const volatile uint32_t *src = (const volatile uint32_t *)(THC_SCB->VTOR & 0xFFFFFF80u);
    for (unsigned i = 0; i < 48u; i++)
        reloc_ivt[i] = src[i];
    THC_SCB->VTOR = (uint32_t)reloc_ivt;
    thc_dsb();
    thc_isb();
}
