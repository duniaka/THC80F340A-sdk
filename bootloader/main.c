#include "thc80f340a.h"
#include "../include/thc_iso.h"

#define BL_MSP        0x20002800u
#define MAX_IVT_ADDR  THC_FLASH_LIMIT
#define VTOR_OK_MAGIC 0xC4FEu
#define PERSIST_MAGIC 0x13371337u   /* at PTR_PAGE+4: single_shot_boot skips the wipe -> boots every reset */
#define PAGE_SIZE     THC_FLASH_PAGE_SIZE
#define RAM_START     THC_RAM_BASE
#define RAM_END       THC_SP_INIT
#define FLASH_START   THC_FLASH_BASE
#define FLASH_END     THC_FLASH_LIMIT

#define FAULT_PTR     0xFF000000u

#define BL_CLA     0x00u

#define OP_PUSH      0x01u
#define OP_SET_ADDR  0x02u
#define OP_READ      0x03u
#define OP_SET_IVT   0x04u
#define OP_JUMP      0x05u
#define OP_FINALIZE  0x06u

#define SW_MEMORY_FULL ((thc_sw_t)0x6A84u)
#define SW_WRONG_DATA  ((thc_sw_t)0x6A80u)
#define SW_WRONG_PARAM ((thc_sw_t)0x6A86u)
#define SW_SECURITY    ((thc_sw_t)0x6982u)
#define SW_MEM_FAIL    ((thc_sw_t)0x6581u)
#define SW_BAD_INS     ((thc_sw_t)0x6D00u)
#define SW_BAD_CLA     ((thc_sw_t)0x6E00u)

static uint32_t be32(const uint8_t *b)
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | b[3];
}

/* T=0 Le convention: P3 == 0 means 256. */
static uint32_t le_len(uint8_t p3)
{
    return p3 ? p3 : 256u;
}

/* Whole-page flash writes only: a flash WPTR must sit on a 512-byte boundary at commit. */
static int flash_page_aligned(uint32_t addr)
{
    return (addr & (PAGE_SIZE - 1u)) == 0u;
}

#ifdef BL_HOST_TEST
#include <assert.h>

int main(void)
{
    assert(be32((const uint8_t[]){ 0x20, 0x00, 0x09, 0x80 }) == 0x20000980u);
    assert(be32((const uint8_t[]){ 0xFF, 0xFF, 0xFF, 0xFF }) == 0xFFFFFFFFu);

    assert(le_len(0) == 256u);
    assert(le_len(1) == 1u);
    assert(le_len(0xFF) == 255u);

    assert(PERSIST_MAGIC != 0xFFFFFFFFu);   /* erased word1 must never read as finalized */

    assert(flash_page_aligned(0x00000000u));
    assert(flash_page_aligned(0x00000200u));
    assert(flash_page_aligned(0x00000400u));
    assert(!flash_page_aligned(0x00000201u));
    assert(!flash_page_aligned(0x000001FFu));
    return 0;
}

#else

extern uint32_t _sram_ivt;
extern uint32_t _bl_end;
extern uint32_t _ptr_page;
#define PTR_PAGE ((uint32_t)&_ptr_page)

static uint32_t VTOR_OK;

static uint8_t  PAGE_BUF[PAGE_SIZE];   /* flash page cache */
static uint32_t CACHE_COUNT;           /* bytes accumulated toward the current flash page */
static uint32_t WPTR;                  /* auto-advancing write pointer */
static uint32_t RPTR;                  /* auto-advancing read pointer  */
static uint8_t  HDR[5];                /* CLA INS P1 P2 P3 -- DMA'd by the pre-armed header receive */

void bootloader_main(void);
static void iso_irq_handler(void);
static void bl_spin(void) { for (;;) { } }

__attribute__((section(".sram_ivt"), used, aligned(128)))
const uint32_t sram_ivt[17] = {
    BL_MSP,
    (uint32_t)bootloader_main + 1u,
    (uint32_t)bl_spin + 1u,
    (uint32_t)bl_spin + 1u,
    0, 0, 0, 0, 0, 0, 0,
    (uint32_t)bl_spin + 1u,
    0, 0,
    (uint32_t)bl_spin + 1u,
    (uint32_t)bl_spin + 1u,
    (uint32_t)iso_irq_handler + 1u,
};

static void halt(void)
{
    __asm__ volatile("cpsid i" ::: "memory");
    for (;;) { }
}

static uint32_t recv_be32(void)
{
    uint8_t b[4];
    thc_recv_bytes(b, 4);
    return be32(b);
}

/* The flash page cache is full (CACHE_COUNT == PAGE_SIZE): burn it to WPTR and advance a page.
 * Left untouched (cache stays full) on a guard failure, so the next PUSH overruns and faults. */
static thc_sw_t commit_page(void)
{
    if (!flash_page_aligned(WPTR))
        return SW_WRONG_DATA;
    if (WPTR < (uint32_t)&_bl_end && VTOR_OK != VTOR_OK_MAGIC)
        return SW_SECURITY;
    if (thc_flash_erase_page((void *)WPTR) != THC_FLASH_OK)
        return SW_MEM_FAIL;
    if (thc_flash_program_bytes((void *)WPTR, PAGE_BUF, PAGE_SIZE) != THC_FLASH_OK)
        return SW_MEM_FAIL;
    WPTR += PAGE_SIZE;
    CACHE_COUNT = 0;
    return THC_SW_OK;
}

/* PUSH streams n bytes (n <= 255) to WPTR. We already sent the T=0 ACK, so every path -- including
 * the reject tail -- consumes exactly n bytes off the wire. Three cases: */
static thc_sw_t cmd_push(uint8_t n)
{
    /* 1. RAM with room: DMA straight to the pointer, advance. */
    if (WPTR >= RAM_START && WPTR + n <= RAM_END) {
        thc_recv_bytes((void *)WPTR, n);
        WPTR += n;
        return THC_SW_OK;
    }
    /* 2. Flash with room in the page cache: buffer, and commit once it's a full page. */
    if (WPTR < FLASH_END && CACHE_COUNT + n <= PAGE_SIZE) {
        thc_recv_bytes(&PAGE_BUF[CACHE_COUNT], n);
        CACHE_COUNT += n;
        return CACHE_COUNT == PAGE_SIZE ? commit_page() : THC_SW_OK;
    }

    /* 3. Anything else: drain the bytes, then reject. A flash-page overrun means the client lost
     *    sync -> poison both pointers so nothing lands until the next SET_ADDR. */
    thc_recv_bytes(PAGE_BUF, n);
    if (WPTR < FLASH_END) {
        CACHE_COUNT = 0;
        WPTR = RPTR = FAULT_PTR;
        return SW_MEMORY_FULL;
    }
    return SW_WRONG_PARAM;
}

static thc_sw_t cmd_set_addr(void)
{
    WPTR = RPTR = recv_be32();
    CACHE_COUNT = 0;
    return THC_SW_OK;
}

static thc_sw_t cmd_set_ivt(void)
{
    uint32_t v = recv_be32();
    if (thc_flash_erase_page((void *)PTR_PAGE) != THC_FLASH_OK)
        return SW_MEM_FAIL;
    if (thc_flash_program_bytes((void *)PTR_PAGE, &v, 4) != THC_FLASH_OK)
        return SW_MEM_FAIL;
    return THC_SW_OK;
}

/* Lock the current single-shot pointer permanent: program the persist marker into word1 of
 * PTR_PAGE. Erased flash there is 0xFFFFFFFF, so this is a pure 1->0 write -- no re-erase.
 * single_shot_boot then boots the pointer on every reset without erasing it. */
static thc_sw_t cmd_finalize(void)
{
    if (*(volatile uint32_t *)PTR_PAGE >= MAX_IVT_ADDR)
        return SW_WRONG_DATA;                 /* no valid pointer set -> nothing to finalize */
    uint32_t magic = PERSIST_MAGIC;
    if (thc_flash_program_bytes((void *)(PTR_PAGE + 4u), &magic, 4) != THC_FLASH_OK)
        return SW_MEM_FAIL;
    return THC_SW_OK;
}

static void iso_irq_register(void)
{
    *THC_NVIC_ICPR = 0xFFFFFFFFu;
    *THC_NVIC_ISER = 1u;
    thc_iso_irq_enable();
}

static void iso_irq_deregister(void)
{
    thc_iso_irq_disable();
    *THC_NVIC_ICER = 0xFFFFFFFFu;
    *THC_NVIC_ICPR = 0xFFFFFFFFu;
}

static void boot_image(uint32_t ivt)
{
    __asm__ volatile("cpsid i" ::: "memory");
    iso_irq_deregister();
    THC_SCB->VTOR = ivt;
    __asm__ volatile("dsb\nisb" ::: "memory");

    uint32_t msp   = ((volatile uint32_t *)ivt)[0];
    uint32_t entry = ((volatile uint32_t *)ivt)[1];
    __asm__ volatile(
        "msr msp, %0\n"
        "bx %1\n"
        :: "r"(msp), "r"(entry));
}

static void process_apdu(void)
{
    uint8_t ins = HDR[1];               /* header already DMA'd in by the pre-armed receive */
    uint8_t p3  = HDR[4];

    if (HDR[0] != BL_CLA) {
        thc_send_sw(SW_BAD_CLA);        /* no data phase: SW directly, no procedure byte */
        return;
    }

    thc_sw_t sw;
    switch (ins) {
    case OP_PUSH:
        thc_send_byte(ins);             /* T=0 procedure byte (ACK == INS): run the data phase */
        sw = cmd_push(p3);
        break;
    case OP_SET_ADDR:
        thc_send_byte(ins);
        sw = cmd_set_addr();
        break;
    case OP_READ: {
        uint32_t le = le_len(p3);       /* case 2: ACK, then Le bytes out, then SW */
        thc_send_byte(ins);
        thc_send_bytes((void *)RPTR, le);
        RPTR += le;
        thc_send_sw(THC_SW_OK);
        return;
    }
    case OP_SET_IVT:
        thc_send_byte(ins);
        sw = cmd_set_ivt();
        break;
    case OP_FINALIZE:
        sw = cmd_finalize();            /* case 1: no data phase, SW returned directly */
        break;
    case OP_JUMP: {
        thc_send_byte(ins);
        uint32_t target = recv_be32();
        thc_send_sw(THC_SW_OK);         /* fully flushed before we tear down and branch */
        boot_image(target);             /* never returns */
        return;
    }
    default:
        sw = SW_BAD_INS;                /* unknown INS: SW directly, no ACK */
        break;
    }
    thc_send_sw(sw);
}

static void iso_irq_handler(void)
{
    thc_iso_irq_disable();
    THC_ISO->DMA_STATUS &= THC_ISO_DMA_RX;
    /* ack the header's RX-done latch -- else the data-phase
       thc_recv_bytes poll sees it still set and exits without
       receiving, desyncing T=0 (mirrors stock handle_iso_irq) */
    process_apdu();
    thc_arm_recv(HDR, sizeof HDR);  /* re-arm the next 5-byte header (OP_JUMP booted away, never here) */
}

static void command_processor(void)
{
    WPTR = RPTR = FAULT_PTR;
    CACHE_COUNT = 0;

    iso_irq_register();

#ifndef BL_SRAM_HIJACK
    thc_iso_init();
    static const uint8_t atr[] = { 0x3B, 0x04, 0x42, 0x4F, 0x4F, 0x54 };
    thc_send_bytes(atr, sizeof atr);
#endif

    thc_arm_recv(HDR, sizeof HDR);  /* arm the first header; its DMA completion is what fires IRQ0 */

    for (;;)
        thc_enter_sleep();
}

static void single_shot_boot(uint32_t cache)
{
    /* Finalized pointer: skip the wipe so it survives every reset. Otherwise erase (one-shot). */
    if (*(volatile uint32_t *)(PTR_PAGE + 4u) != PERSIST_MAGIC &&
        thc_flash_erase_page((void *)PTR_PAGE) != THC_FLASH_OK) {
        return;
    }
    boot_image(cache);
}

void bootloader_main(void)
{

#ifdef BL_SRAM_HIJACK
    thc_send_sw(THC_SW_OK);
#endif

    VTOR_OK = 0;
    const volatile uint32_t *ivt = (const volatile uint32_t *)THC_SCB->VTOR;
    if ((uint32_t)ivt == (uint32_t)&_sram_ivt &&
        ivt[0]  == BL_MSP &&
        ivt[1]  == ((uint32_t)bootloader_main | 1u))
        VTOR_OK = VTOR_OK_MAGIC;

#ifndef BL_SRAM_HIJACK
    /* Persistent single-shot boot pointer. The RAM test image skips this entirely: its _ptr_page
     * aliases stock flash, which on a real card holds non-0xFF data -> a bogus boot + page erase. */
    uint32_t target = *(volatile uint32_t *)PTR_PAGE;
    if (target < MAX_IVT_ADDR) {
        single_shot_boot(target);
    }
#endif

    command_processor();
}

#endif
