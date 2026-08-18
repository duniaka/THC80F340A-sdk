// CRC-16 accelerator registers.
#ifndef THC_CRC_H
#define THC_CRC_H

#include <stdint.h>

typedef struct {
    // Write bytes in, read result out.
    // Two reads needed, low byte first.

    // TODO: Claude tells me it is CRC-16/KERMIT, though the datasheet says it is CRC16/CCITT

    // CRC-16/KERMIT, poly 0x8408 reflected, init 0.
    // Stock firmware reads it byte-swapped though.
    // Reproduce that quirk for CRC-VERIFY, INS 0x30.
    volatile uint32_t DATA;
    volatile uint32_t _reserved0;
    volatile uint32_t UNKNOWN0;  // real, nonzero (0x6), unreferenced
} thc_crc_t;
#define THC_CRC ((thc_crc_t *)0x40041000u)

static inline uint16_t thc_crc16(const void *data, uint32_t len)
{
    const uint8_t *src = (const uint8_t *)data;
    (void)THC_CRC->DATA;
    for (uint32_t i = 0; i < len; i++) {
        THC_CRC->DATA = src[i];
    }
    return (uint16_t)((THC_CRC->DATA << 8) + THC_CRC->DATA);
}

#endif // THC_CRC_H
