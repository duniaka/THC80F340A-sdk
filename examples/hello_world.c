#include "thc80f340a.h"

void _start(void);
static void trap(void) { for (;;) { } }

__attribute__((section(".isr_vector"), used))
const uint32_t vectors[] = {
    THC_SP_INIT,
    (uint32_t)_start + 1u,
    (uint32_t)trap + 1u,
    (uint32_t)trap + 1u,
    0, 0, 0, 0, 0, 0, 0,
    (uint32_t)trap + 1u,
    0, 0,
    (uint32_t)trap + 1u,
    (uint32_t)trap + 1u,
    (uint32_t)trap + 1u,
};

void _start(void)
{
    static const uint8_t atr[] = { 0x3B, 0x00 };

    *THC_NVIC_ICER = 0xFFFFFFFFu;
    *THC_NVIC_ICPR = 0xFFFFFFFFu;

    thc_iso_init();
    thc_send_bytes(atr, sizeof atr);

    for (;;) {
        uint8_t hdr[5];
        thc_enter_sleep();
        thc_recv_bytes(hdr, sizeof hdr);
        thc_send_sw(THC_SW_OK);
    }
}
