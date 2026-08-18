// FLASH: flash controller registers.
// Depends on thc_core.h for thc_dsb().
// thc_dsb is the pre-trigger barrier.
#ifndef THC_FLASH_H
#define THC_FLASH_H

#include <stdint.h>
#include "thc_core.h"

typedef struct {
    volatile uint32_t STATUS;    // bit0 done, bit1/2 ERR_*, bit4 unconfirmed
    volatile uint32_t KEY1;      // unlock byte 1, see KEY1_*
    volatile uint32_t KEY2;      // unlock byte 2, see KEY2_*
    volatile uint32_t CMD;       // op select, see CMD_*
    volatile uint32_t _reserved0[4];
    volatile uint32_t BUF_ADDR;  // buffered-write source, CMD_BUFFERED only
    volatile uint32_t BUF_LEN;   // buffered-write length, always 0x80
} thc_flash_t;
#define THC_FLASH ((thc_flash_t *)0x40020000u)
#define THC_FLASH_BASE      0x00000000u
#define THC_FLASH_LIMIT     0x00055000u
#define THC_FLASH_PAGE_SIZE 0x200u

#define THC_FLASH_STATUS_DONE  0x1u
#define THC_FLASH_STATUS_ERR_A 0x2u
#define THC_FLASH_STATUS_ERR_B 0x4u

#define THC_FLASH_CMD_PROGRAM  0x1u
#define THC_FLASH_CMD_ERASE    0x11u
#define THC_FLASH_CMD_BUFFERED 0x40u

#define THC_FLASH_KEY1_ERASE 0x55u
#define THC_FLASH_KEY2_ERASE 0xAAu
#define THC_FLASH_KEY1_WRITE 0xAAu  // program and buffered share this order
#define THC_FLASH_KEY2_WRITE 0x55u

// Return codes for the erase/program helpers.
#define THC_FLASH_OK         1u
#define THC_FLASH_ERR_OP     2u  // STATUS ERR_A: hardware op fault (YCOS FL_OP_ERR)
#define THC_FLASH_ERR_VERIFY 4u  // erase: ERR_B check-0xFF fail; program: readback mismatch (YCOS FL_CHKFF_ERR)

// Returns THC_FLASH_OK / THC_FLASH_ERR_OP / THC_FLASH_ERR_VERIFY.
static inline uint32_t thc_flash_erase_page(void *addr)
{
    THC_FLASH->CMD  = THC_FLASH_CMD_ERASE;
    THC_FLASH->KEY1 = THC_FLASH_KEY1_ERASE;
    THC_FLASH->KEY2 = THC_FLASH_KEY2_ERASE;
    thc_dsb();
    *(volatile uint8_t *)addr = 0xFFu;
    while (!(THC_FLASH->STATUS & THC_FLASH_STATUS_DONE)) { }
    THC_FLASH->STATUS &= THC_FLASH_STATUS_DONE;
    if (THC_FLASH->STATUS & THC_FLASH_STATUS_ERR_A) {
        THC_FLASH->STATUS &= THC_FLASH_STATUS_ERR_A;
        return THC_FLASH_ERR_OP;
    }
    if (THC_FLASH->STATUS & THC_FLASH_STATUS_ERR_B) {
        THC_FLASH->STATUS &= THC_FLASH_STATUS_ERR_B;
        return THC_FLASH_ERR_VERIFY;
    }
    return THC_FLASH_OK;
}

// Unlock and write once per byte.
// Verifies by readback after all bytes.
// Returns THC_FLASH_OK / THC_FLASH_ERR_OP (hw) / THC_FLASH_ERR_VERIFY (mismatch).
static inline uint32_t thc_flash_program_bytes(void *dst, const void *src, uint32_t len)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < len; i++) {
        THC_FLASH->CMD  = THC_FLASH_CMD_PROGRAM;
        THC_FLASH->KEY1 = THC_FLASH_KEY1_WRITE;
        THC_FLASH->KEY2 = THC_FLASH_KEY2_WRITE;
        thc_dsb();
        d[i] = s[i];
        while (!(THC_FLASH->STATUS & THC_FLASH_STATUS_DONE)) { }
        THC_FLASH->STATUS &= THC_FLASH_STATUS_DONE;
        if (THC_FLASH->STATUS & THC_FLASH_STATUS_ERR_A) {
            THC_FLASH->STATUS &= THC_FLASH_STATUS_ERR_A;
            return THC_FLASH_ERR_OP;
        }
    }
    for (uint32_t i = 0; i < len; i++) {
        if (d[i] != s[i]) return THC_FLASH_ERR_VERIFY;
    }
    return THC_FLASH_OK;
}

#endif // THC_FLASH_H
