// PWR: power and sleep control.
// Depends on thc_wdt.h.
// thc_enter_sleep touches both WDTs too.
#ifndef THC_PWR_H
#define THC_PWR_H

#include <stdint.h>
#include "thc_wdt.h"

typedef struct {
    volatile uint32_t CTRL;      // write THC_PWR_SLEEP to sleep, self-clears
    volatile uint32_t _reserved0[3];
    // bit7 set by init_iso_hw
    // when bit0 was clear
    volatile uint32_t CLK_CTRL;
} thc_pwr_t;
#define THC_PWR ((thc_pwr_t *)0x40000000u)

#define THC_PWR_SLEEP            0x1u   // CTRL: write to enter sleep, self-clears
#define THC_PWR_CLK_CTRL_VALID   0x1u   // CLK_CTRL bit0: clock valid; if clear, force default
#define THC_PWR_CLK_CTRL_DEFAULT 0x80u  // CLK_CTRL bit7: value init_iso_hw writes when invalid

// Matches stock firmware enter_sleep_mode, 0x53858.
static inline void thc_enter_sleep(void)
{
    uint32_t wdt_a = THC_WDT_A->CTRL;
    uint32_t wdt_b = THC_WDT_B->CTRL;
    THC_WDT_A->CTRL = 0;
    THC_WDT_B->CTRL = 0;
    THC_PWR->CTRL = THC_PWR_SLEEP;
    while (THC_PWR->CTRL == THC_PWR_SLEEP) { }
    THC_WDT_A->CTRL = wdt_a;
    THC_WDT_B->CTRL = wdt_b;
}

#endif // THC_PWR_H
