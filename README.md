# THC80F340A SDK

[![License: PolyForm Noncommercial 1.0.0](https://img.shields.io/badge/license-PolyForm%20Noncommercial%201.0.0-blue)](LICENSE.md)
![Arch](https://img.shields.io/badge/arch-Cortex--M0%20(ARMv6--M)-informational)
![Status](https://img.shields.io/badge/status-research%20%2F%20experimental-orange)

A bare-metal SDK and custom bootloader for the **THC80F340A**, an Arm Cortex-M0 smart-card (aka CPU card): a hardware abstraction layer, linker scripts, a bootloader that can run your own code, and the host-side tooling to get code onto the chip.

It has NOR flash (erase-only, with a full-page reset) with the size of 0x55000 bytes (340kb), and around 0x2A00 of RAM.

> **⚠️ Research repo, not a product.** No datasheet section covers any of this — it was all recovered by reverse-engineering the stock firmware pulled off the chip: static analysis in Ghidra to map the stock bootloader. It has not seen production testing across silicon revisions, or got audited for security.

## What can I do with this repo?

You can write self-contained apps (no libc, no crt0) that fully control the periphery of the device. Each defines its own `.isr_vector` and its own `_start`

```c
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
```

`flash_after_bl.ld` links it into the bootloader's app partition at flash `0x8000` and pads it to a whole 512-byte page. Apps are not bringing parts of the bootloader, you would need to init all the periphery yourself.

## Why this exists

The world is locked down to Java cards – you can control _only_ the Java VM you are loaded in. But, ARM "Cortex-M0"-like cards in the wild are.. extremely cool – you can send your ATR or even decide a default ISO7816 speed (instead of 10752 bauds). Maybe, even to make a game that you could play with a U(S)ART adapter.

I have asked a seller for code samples for this model. They didn't provide any. The chip ships with a precompiled stock OS, so I found a way in into the original bootloader, and made my own bootloader on top of it that you can actually test your code without a fear of breaking the card (boot-once-once feature).

## Getting help

Open an issue on this repo — questions, bug reports, and "this bricked my card" reports are all welcome. There's no separate support channel.

## License

Source-available under the [PolyForm Noncommercial 1.0.0](LICENSE.md) license: free for hobby, personal, research, educational, and nonprofit use. Commercial use is not permitted.

## Flash map of the stock bootloader

- **0x00** – IVT table (ARM Cortex-M0-like) that stores data on from where to boot from, where to bring the interrupts.
- **`0x1000`** – default factory routine, that works as an extension to the bootloader. You could replace it with your own code and it'd become the part of the stock bootloader. By default, it has some of factory card testing code, like filling regions and checking if some regions of the card are blank.
- **`0x542B8`** – the boot-time dispatcher. It reads the word at `0x1000`; if it equals `0x01234567`, that's treated as "custom command handler installed" and it falls through to `0x1278`. I have decided to ditch it, as reverse engineering it is a pain.

## How we flash our brand-new bootloader

1. We patch that part at **0x1000**, at **0x1554** we patch a command with our own **JMP** instruction. So calling a factory test routine sends us straight into our shell-code that does the IVT remapping.
2. We trigger it: the shellcode remaps IVT table to rom the default **0x00** location to RAM. So we can write to **0x00** without bricking the card. The card has NOR memory, so before updating the vector table, we **have to** erase it first. Erasing IVT table in the runtime kills the card, trust me – we lose the IRQ0 vector responsible for the APDU comm link. Our last link.
3. Then we use the `set-base` and `write` APDU commands to overwrite all of the stock bootloader with our own.
4. On the next boot our bootloader is going to welcome us with our own command-set. Now you can upload your own code to 0x8000

Our bootloader has a SRAM-only version for dev purposes – it resets on a reboot. So you can keep the stock bootloader.

## Layout

| Path          | What                                                                                                                                                                                                                                                   |
| ------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `include/`    | The drivers — `thc80f340a.h` plus one header per peripheral (`thc_iso.h`, `thc_flash.h`, `thc_crc.h`, `thc_trng.h`, `thc_pwr.h`, `thc_wdt.h`, `thc_mem.h`, `thc_core.h`). Every named register/constant is confirmed on real hardware, but not tested. |
| `link/`       | Linker scripts, one per load scenario (app-after-bootloader, SRAM bootloader).                                                                                                                                                                         |
| `bootloader/` | The custom bootloader — builds as a flash ROM (runs at `0x0`) or a RAM image.                                                                                                                                                                          |
| `examples/`   | Worked apps. `hello_world.c` is a minimal ISO7816 responder.                                                                                                                                                                                           |
| `tools/`      | Host tooling: `generate_pf.py` (emits upload scripts) and `.pf` scripts.                                                                                                                                                                               |
| `build/`      | Generated output (gitignored). All build products land here.                                                                                                                                                                                           |

## Requirements

- A bare-metal `arm-*-eabi-gcc` toolchain.
  `-mcpu=cortex-m0`.
- **To upload to a physical card:** [`pf-loader`](#uploading-code-to-the-card) on your `PATH`, plus
  a card + a locally running `pcscd`.
- **For development, don't rely on macOS's built-in CCID driver** — it's flaky and forces external readers to behave extremely weirdly, and cause multiple resets.
  Install and use a standalone CCID driver
  ([pcsc-lite's `ccid`](https://ccid.apdu.fr/) via Homebrew, or similar) instead and point `pcscd`
  at it.

Override tool names on the make line if yours differ: `make GCC=... OBJCOPY=... PYTHON=... PFLOADER=...`

## Build

```sh
make
```

Outputs (all under `build/`): `examples/*.bin`, `bootloader.bin` (flash `0x0`),
`bootloader-flash.bin` (full padded flash image), `bootloader-sram.bin` (runs in RAM at `0x20001800`), and the `.pf` upload scripts.

## Uploading code to the card

Uploads are driven by **`.pf` scripts** — a plain-text list of APDU exchanges:

```
# comment                     a hash-prefixed comment line
:label                        a progress label printed as the loader runs
>00 50 00 00 00               a frame to send (SET-BASE here)
<90 00                        the expected response
<XX XX 90 00                  XX = wildcard byte (don't-care)
```

```sh
make flash_bootloader       # install the bootloader at flash 0x0 (persistent)
make sram_load_bootloader   # load the bootloader into RAM and run it (this session only)
make test_bootloader        # exercise the bootloader's opcode set (BL must already be running)
```

A fresh PC/SC connect cold-boots the card back to stock and clears RAM, so a SRAM load must be
redone each session before any command that assumes the bootloader is running.