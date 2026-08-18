// ISO7816 UART and timer block.
// Raw send/recv, status word, protocol/timer helpers.
#ifndef THC_ISO_H
#define THC_ISO_H

#include <stdint.h>
#include "thc_pwr.h"

typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t CONFIG;    // boot value 0x17, init_iso_hw
    volatile uint32_t _reserved0;
    volatile uint32_t STATUS;
    volatile uint32_t PROTO;     // F/D baud code (1 of 11 datasheet F/D vals), set_proto_byte/PPS -- not T=0/T=1
    volatile uint32_t TX_DATA;
    volatile uint32_t BAUD;      // written once, only in a diagnostic mode (to 2/3); runtime role unknown -- PROTO selects baud
    volatile uint32_t GUARD;     // boot value 0xFF, init_iso_hw
    // 0x20-0x2c: nonzero on silicon, but binary disassembly shows firmware never accesses it
    // (no address literal, no base+offset load) -- purpose unknown. The datasheet's ETU/NULL-byte
    // timer is the referenced TIMER_* cluster below, not this.
    volatile uint32_t _unknown0x20[4];
    volatile uint32_t DMA_CTRL;   // THC_ISO_DMA_TX/RX to start
    volatile uint32_t DMA_STATUS; // TX/RX/RX_BUSY when set
    volatile uint32_t DMA_ADDR;
    volatile uint32_t DMA_LEN;
    volatile uint32_t RX_COUNT;   // live count during a DMA RX
    volatile uint32_t IRQ_MASK;   // write IRQ_MASK_ALL after handling
    volatile uint32_t _reserved1[2];
    volatile uint32_t TIMER_CTRL;      // bit0 enable, bits1-2 counting mode, bit3 done-latch
    volatile uint32_t TIMER_RELOAD_A;  // live counter (YCOS ISOTDAT), preloaded equal to RELOAD_B
    volatile uint32_t TIMER_RELOAD_B;  // reload latch (YCOS ISOTRLD), preloaded equal to RELOAD_A
    volatile uint32_t TIMER_MODE;      // bit0: mask NULL-byte IRQ before arming (YCOS ISOTMSK), so DONE is polled
    volatile uint32_t TIMER_PRESCALE;
} thc_iso_t;
#define THC_ISO ((thc_iso_t *)0x40080000u)

// DMA_CTRL/DMA_STATUS bit values.
// RX_BUSY only seen on DMA_STATUS.
// Set while a variable-length RX arrives.
// Example: a PPS request, see handle_iso_irq.
// IRQ_MASK_ALL: fixed value written after handling.
#define THC_ISO_DMA_TX      0x1u
#define THC_ISO_DMA_RX      0x2u
#define THC_ISO_DMA_RX_BUSY 0x4u
#define THC_ISO_IRQ_MASK_ALL (THC_ISO_DMA_TX | THC_ISO_DMA_RX | THC_ISO_DMA_RX_BUSY)

#define THC_ISO_TIMER_EN   0x1u  // bit0: enable/start
#define THC_ISO_TIMER_DONE 0x8u  // bit3: latches on at expiry
#define THC_ISO_TIMER_MODE1     0x2u  // bit1: counting mode 1 (guard timer)
#define THC_ISO_TIMER_MODE_MASK 0x6u  // bits1-2: counting-mode select
#define THC_ISO_TIMER_MODE_ARM  0x1u  // TIMER_MODE bit0: mask NULL IRQ, set before arming

// Register boot/init values, written by thc_iso_init / stock init_iso_hw.
#define THC_ISO_CONFIG_INIT 0x17u
#define THC_ISO_GUARD_INIT  0xFFu

#define THC_ISO_CTRL_TX      0x20u  // bit5: TX gate, set around a send
#define THC_ISO_CTRL_BUSY    0x10u  // bit4: busy, polled before reconfig
#define THC_ISO_STATUS_TXRDY 0x1u   // bit0: TX ready/idle

// Supported F/D (Fi/Di) baud codes -- k_proto_table @ flash 0x5388d, matches datasheet.
// PPS accepts only these; 0x11 (F=372,D=1) is the reset default.
#define THC_ISO_FD_DEFAULT 0x11u
#define THC_ISO_FD_CODES { \
    0x11u, 0x12u, 0x13u, 0x18u, 0x91u, 0x92u, 0x93u, 0x94u, 0x95u, 0x96u, 0x97u }

// Raw ISO7816 primitives.
// Byte-for-byte stock firmware register sequences.
// No framing or timeout built in.
// thc_recv_bytes waits forever if nothing arrives.
static inline void thc_send_byte(uint8_t value)
{
    THC_ISO->CTRL |= THC_ISO_CTRL_TX;
    THC_ISO->TX_DATA = value;
    while (!(THC_ISO->STATUS & THC_ISO_STATUS_TXRDY)) { }
    THC_ISO->CTRL &= ~THC_ISO_CTRL_TX;
}

static inline void thc_send_bytes(const void *src, uint32_t len)
{
    THC_ISO->CTRL |= THC_ISO_CTRL_TX;
    THC_ISO->DMA_ADDR = (uint32_t)src;
    THC_ISO->DMA_LEN = len;
    THC_ISO->DMA_CTRL |= THC_ISO_DMA_TX;
    while (!(THC_ISO->DMA_STATUS & THC_ISO_DMA_TX)) { }
    THC_ISO->DMA_STATUS &= THC_ISO_DMA_TX;
    THC_ISO->CTRL &= ~THC_ISO_CTRL_TX;
}

static inline void thc_recv_bytes(void *dest, uint32_t len)
{
    THC_ISO->DMA_ADDR = (uint32_t)dest;
    THC_ISO->DMA_LEN = len;
    THC_ISO->DMA_CTRL |= THC_ISO_DMA_RX;
    while (!(THC_ISO->DMA_STATUS & THC_ISO_DMA_RX)) { }
    THC_ISO->DMA_STATUS &= THC_ISO_DMA_RX;
}

// Arm a receive DMA and return WITHOUT waiting -- completion raises the ISO RX IRQ. Mirrors stock
// arm_header_recv: clear the RX-done latch, point the DMA at dest, unmask RX (IRQ_MASK is active-low,
// 0xFFFFFFFD == only RX enabled), then start. The ISR reads dest and re-arms. This is the only way
// the IRQ ever fires on real silicon; a bare unmask with no armed DMA never completes.
static inline void thc_arm_recv(void *dest, uint32_t len)
{
    THC_ISO->DMA_STATUS &= THC_ISO_DMA_RX;
    THC_ISO->DMA_ADDR = (uint32_t)dest;
    THC_ISO->DMA_LEN = len;
    THC_ISO->IRQ_MASK = ~THC_ISO_DMA_RX;
    THC_ISO->DMA_CTRL = THC_ISO_DMA_RX;
}

// Status word, SW1 then SW2.
typedef uint16_t thc_sw_t;
#define THC_SW_OK ((thc_sw_t)0x9000u)

static inline void thc_send_sw(thc_sw_t sw)
{
    thc_send_byte((uint8_t)(sw >> 8));
    thc_send_byte((uint8_t)sw);
}

// Matches stock firmware answer_pps_request, 0x530b8.
static inline void thc_set_protocol(uint8_t proto)
{
    THC_ISO->PROTO = proto;
}

// True if fd is one of the 11 supported F/D codes (mirrors set_proto_byte's table scan).
static inline int thc_iso_fd_supported(uint8_t fd)
{
    static const uint8_t codes[] = THC_ISO_FD_CODES;
    for (unsigned i = 0; i < sizeof codes; i++)
        if (codes[i] == fd)
            return 1;
    return 0;
}

// Matches stock firmware set_timer_mode, 0x5305c.
static inline void thc_set_timer_mode(int mode, uint32_t prescale, uint32_t reload)
{
    if (mode != 1) {
        THC_ISO->TIMER_CTRL &= 0xFFu & ~THC_ISO_TIMER_EN;  // 0xFE: byte reg, clears bit0
        return;
    }
    THC_ISO->TIMER_RELOAD_B = reload;
    THC_ISO->TIMER_RELOAD_A = reload;
    THC_ISO->TIMER_PRESCALE = prescale;
    THC_ISO->TIMER_MODE |= THC_ISO_TIMER_MODE_ARM;
    THC_ISO->TIMER_CTRL =
        (THC_ISO->TIMER_CTRL & (0xFFu & ~THC_ISO_TIMER_MODE_MASK)) | THC_ISO_TIMER_EN;  // 0xF9|1
}

// Matches stock firmware arm_guard_timer, 0x52e6c.
static inline void thc_arm_guard_timer(uint32_t reload)
{
    THC_ISO->TIMER_RELOAD_A = reload;
    THC_ISO->TIMER_RELOAD_B = reload;
    THC_ISO->TIMER_MODE |= THC_ISO_TIMER_MODE_ARM;
    THC_ISO->TIMER_CTRL = THC_ISO_TIMER_EN | THC_ISO_TIMER_MODE1;
    while (!(THC_ISO->TIMER_CTRL & THC_ISO_TIMER_DONE)) { }
    THC_ISO->TIMER_CTRL |= THC_ISO_TIMER_DONE;
    THC_ISO->TIMER_CTRL &= ~THC_ISO_TIMER_EN;
}

static inline void thc_iso_irq_enable(void)
{
    THC_ISO->IRQ_MASK &= ~THC_ISO_DMA_RX;
}

static inline void thc_iso_irq_disable(void)
{
    THC_ISO->IRQ_MASK = THC_ISO_IRQ_MASK_ALL;
}

static inline void thc_iso_init(void)
{
    uint32_t clk = THC_PWR->CLK_CTRL;
    if (!(clk & THC_PWR_CLK_CTRL_VALID))
        clk = THC_PWR_CLK_CTRL_DEFAULT;
    THC_PWR->CLK_CTRL = clk;
    THC_ISO->GUARD  = THC_ISO_GUARD_INIT;
    THC_ISO->CONFIG = THC_ISO_CONFIG_INIT;
}

#endif // THC_ISO_H
