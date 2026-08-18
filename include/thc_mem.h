// RAM layout and last APDU header.
// Not a peripheral, but shared facts.
// Every other header/example relies on these.
#ifndef THC_MEM_H
#define THC_MEM_H

#include <stdint.h>

#define THC_RAM_BASE  0x20000000u
#define THC_RAM_END   0x200029FFu
//last real byte -- writes past here return SW=9000 but silently don't persist
// Real boundary RAM self-test writes across.
// Avoid placing scratch data here.
#define THC_RAM_BANK1 0x20001000u
// Real reset SP value, grows downward.
#define THC_SP_INIT   0x20002A00u

typedef struct {
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    uint8_t lc;
} thc_apdu_header_t;
#define THC_LAST_APDU ((volatile thc_apdu_header_t *)0x20000977u)

#endif // THC_MEM_H
