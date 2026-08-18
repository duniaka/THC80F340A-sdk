// WDT_A/WDT_B watchdog control registers.
// No wrappers: bit-level semantics unconfirmed.
// See README for reasoning.
#ifndef THC_WDT_H
#define THC_WDT_H

#include <stdint.h>

typedef struct {
    volatile uint32_t CTRL;
} thc_wdt_t;
#define THC_WDT_A ((thc_wdt_t *)0x40060000u)  // observed CTRL=0x3
#define THC_WDT_B ((thc_wdt_t *)0x40061000u)  // observed CTRL=0x1

#endif // THC_WDT_H
