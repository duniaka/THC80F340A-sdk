// TRNG: true random number generator.
#ifndef THC_TRNG_H
#define THC_TRNG_H

#include <stdint.h>

typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t DATA;
    volatile uint32_t STATUS;    // bit0 = ready
} thc_trng_t;
#define THC_TRNG ((thc_trng_t *)0x40065000u)

// bit0 is an active-low enable: clear to run, set to stop (matches YCOS RNGCTL).
#define THC_TRNG_CTRL_DISABLE 0x1u
#define THC_TRNG_STATUS_READY 0x1u  // bit0

// Matches stock firmware b0_read_random_bytes, 0x11dc.
// Reads len bytes, polling STATUS each byte.
// A bare DATA read returns stale data.
// This handshake produces fresh entropy.
static inline void thc_trng_read(void *dest, uint32_t len)
{
    uint8_t *out = (uint8_t *)dest;
    THC_TRNG->CTRL &= ~THC_TRNG_CTRL_DISABLE;
    while (len--) {
        while (!(THC_TRNG->STATUS & THC_TRNG_STATUS_READY)) { }
        *out++ = (uint8_t)THC_TRNG->DATA;
    }
    THC_TRNG->CTRL |= THC_TRNG_CTRL_DISABLE;
}

#endif // THC_TRNG_H
