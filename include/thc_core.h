// ARMv6-M System Control Space, DDI0419C.
// Architectural, not TMC-specific.
// THC80F340A specifics live in thc80f340a.h.
// Confirmed via direct real-silicon reads.
#ifndef THC_CORE_H
#define THC_CORE_H

#include <stdint.h>

// System Control Block, 0xE000ED00.
// CPUID reads 0x410CC300, SecurCore SC000.
// Distinct from Cortex-M0's 0xC20.
typedef struct {
    volatile const uint32_t CPUID;
    volatile uint32_t ICSR;
    volatile uint32_t VTOR;      // writable + honored on real silicon; needs thc_dsb() after write (see examples/vtor_irq.c)
    volatile uint32_t AIRCR;     // write VECTKEY 0x05FA in bits[31:16] (reads back as VECTKEYSTAT 0xFA05)
    volatile uint32_t SCR;
    volatile uint32_t CCR;       // confirmed 0x00000208, UNALIGN_TRP and bit9 set
    volatile uint32_t _reserved0;
    volatile uint32_t SHPR2;
    volatile uint32_t SHPR3;
    volatile uint32_t SHCSR;     // Debug Extension only, reads 0
    volatile uint32_t _reserved1[2];
    volatile uint32_t DFSR;      // Debug Extension only, reads 0
} thc_scb_t;
#define THC_SCB   ((thc_scb_t *)0xE000ED00u)
#define THC_ACTLR (*(volatile uint32_t *)0xE000E008u)

// AIRCR fields, for thc_system_reset().
#define THC_AIRCR_VECTKEY     0x05FAu      // write key, bits[31:16]; readback is 0xFA05
#define THC_AIRCR_SYSRESETREQ (1u << 2)

// NVIC, 0xE000E100.
// One ISER/ICER/ISPR/ICPR word each, ARMv6-M.
// Up to 32 IRQ lines architecturally.
// Only 4 real: ISER reads back 0xF.
// Bits 4-31 always read zero.
// Only IRQ0 used by stock firmware.
// IRQ0 is the combined ISO interrupt.
#define THC_NVIC_ISER ((volatile uint32_t *)0xE000E100u)
#define THC_NVIC_ICER ((volatile uint32_t *)0xE000E180u)
#define THC_NVIC_ISPR ((volatile uint32_t *)0xE000E200u)
#define THC_NVIC_ICPR ((volatile uint32_t *)0xE000E280u)
// IPR0..IPR7, 8 words total.
// One priority byte per IRQ line.
// All confirmed 0 (highest) at read.
#define THC_NVIC_IPR0 ((volatile uint32_t *)0xE000E400u)

static inline void thc_irq_enable(uint32_t irq)
{
    *THC_NVIC_ISER = 1u << irq;
}

static inline void thc_irq_disable(uint32_t irq)
{
    *THC_NVIC_ICER = 1u << irq;
}

static inline int thc_irq_is_pending(uint32_t irq)
{
    return (int)((*THC_NVIC_ISPR >> irq) & 1u);
}

static inline void thc_irq_clear_pending(uint32_t irq)
{
    *THC_NVIC_ICPR = 1u << irq;
}

static inline void thc_irq_set_pending(uint32_t irq)
{
    *THC_NVIC_ISPR = 1u << irq;
}

// IPR is word-access-only on ARMv6-M: read-modify-write the byte for this IRQ.
// ponytail: writes the full 8-bit field; number of implemented high bits is
// unconfirmed on this silicon (priorities read 0 today) -- measure if priority
// ordering ever actually matters. External IRQs only (no system-handler path).
static inline void thc_irq_set_priority(uint32_t irq, uint32_t prio)
{
    volatile uint32_t *ipr = THC_NVIC_IPR0 + (irq >> 2);
    uint32_t shift = (irq & 3u) * 8u;
    *ipr = (*ipr & ~(0xFFu << shift)) | ((prio & 0xFFu) << shift);
}

static inline uint32_t thc_irq_get_priority(uint32_t irq)
{
    volatile uint32_t *ipr = THC_NVIC_IPR0 + (irq >> 2);
    return (*ipr >> ((irq & 3u) * 8u)) & 0xFFu;
}

// Matches stock firmware sync_memory, 0x52c00.
// Required before any flash trigger write.
static inline void thc_dsb(void)
{
    __asm__ volatile("dsb" ::: "memory");
}

// Instruction barrier: flush the pipeline so later fetches see prior writes (e.g. a new VTOR).
static inline void thc_isb(void)
{
    __asm__ volatile("isb" ::: "memory");
}

// System reset via AIRCR SYSRESETREQ. Does not return.
// ponytail: SYSRESETREQ behavior not yet confirmed on real silicon -- verify it resets.
static inline void thc_system_reset(void)
{
    thc_dsb();
    THC_SCB->AIRCR = (THC_AIRCR_VECTKEY << 16) | THC_AIRCR_SYSRESETREQ;
    thc_dsb();
    for (;;) { }
}

// Generic MMIO access, any address readable.
// Unmapped reads return 0, never fault.
static inline uint32_t thc_reg_read32(uint32_t addr)
{
    return *(volatile uint32_t *)addr;
}

static inline void thc_reg_write32(uint32_t addr, uint32_t value)
{
    *(volatile uint32_t *)addr = value;
}

#endif // THC_CORE_H
