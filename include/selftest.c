/* Compile-time check that struct layouts match confirmed real hardware offsets -- a wrong
 * field or missing padding word is a compile error here, not a bug discovered on a real card.
 * `make selftest` runs this. No output = pass.
 */
#include <stddef.h>
#include "thc80f340a.h"

#define CHECK(type, field, expect) \
    _Static_assert(offsetof(type, field) == (expect), #type "." #field " offset wrong")

CHECK(thc_scb_t, CPUID, 0x00);
CHECK(thc_scb_t, ICSR, 0x04);
CHECK(thc_scb_t, VTOR, 0x08);
CHECK(thc_scb_t, AIRCR, 0x0C);
CHECK(thc_scb_t, SCR, 0x10);
CHECK(thc_scb_t, CCR, 0x14);
CHECK(thc_scb_t, SHPR2, 0x1C);
CHECK(thc_scb_t, SHPR3, 0x20);
CHECK(thc_scb_t, SHCSR, 0x24);
CHECK(thc_scb_t, DFSR, 0x30);

CHECK(thc_pwr_t, CTRL, 0x00);
CHECK(thc_pwr_t, CLK_CTRL, 0x10);

CHECK(thc_flash_t, STATUS, 0x00);
CHECK(thc_flash_t, KEY1, 0x04);
CHECK(thc_flash_t, KEY2, 0x08);
CHECK(thc_flash_t, CMD, 0x0C);
CHECK(thc_flash_t, BUF_ADDR, 0x20);
CHECK(thc_flash_t, BUF_LEN, 0x24);

CHECK(thc_crc_t, DATA, 0x00);
CHECK(thc_crc_t, UNKNOWN0, 0x08);

CHECK(thc_trng_t, CTRL, 0x00);
CHECK(thc_trng_t, DATA, 0x04);
CHECK(thc_trng_t, STATUS, 0x08);

CHECK(thc_iso_t, CTRL, 0x00);
CHECK(thc_iso_t, CONFIG, 0x04);
CHECK(thc_iso_t, STATUS, 0x0C);
CHECK(thc_iso_t, PROTO, 0x10);
CHECK(thc_iso_t, TX_DATA, 0x14);
CHECK(thc_iso_t, BAUD, 0x18);
CHECK(thc_iso_t, GUARD, 0x1C);
CHECK(thc_iso_t, _unknown0x20, 0x20);
CHECK(thc_iso_t, DMA_CTRL, 0x30);
CHECK(thc_iso_t, DMA_STATUS, 0x34);
CHECK(thc_iso_t, DMA_ADDR, 0x38);
CHECK(thc_iso_t, DMA_LEN, 0x3C);
CHECK(thc_iso_t, RX_COUNT, 0x40);
CHECK(thc_iso_t, IRQ_MASK, 0x44);
CHECK(thc_iso_t, TIMER_CTRL, 0x50);
CHECK(thc_iso_t, TIMER_RELOAD_A, 0x54);
CHECK(thc_iso_t, TIMER_RELOAD_B, 0x58);
CHECK(thc_iso_t, TIMER_MODE, 0x5C);
CHECK(thc_iso_t, TIMER_PRESCALE, 0x60);

_Static_assert(sizeof(thc_apdu_header_t) == 5, "thc_apdu_header_t must be exactly 5 bytes");

int main(void) { return 0; }
