GCC     ?= arm-none-eabi-gcc
OBJCOPY ?= arm-none-eabi-objcopy

HERE := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
B    := $(HERE)../build

CFLAGS := -mcpu=cortex-m0 -mthumb -ffreestanding -fno-builtin -fno-jump-tables \
          -nostdlib -nostartfiles -I$(HERE)../include -Os
LDFLAGS := -T$(HERE)bootloader.ld -Wl,--build-id=none

SRCS := $(HERE)boot.s $(HERE)micro.c $(HERE)main.c
DEPS := $(HERE)bootloader.ld $(wildcard $(HERE)../include/*.h)

FLASH_SIZE := 0x55000

all: $(B)/bootloader.bin
bootloader.bin: $(B)/bootloader.bin
firmware.bin: $(B)/firmware.bin
bootloader-flash.bin: $(B)/bootloader-flash.bin
bootloader-sram.bin: $(B)/bootloader-sram.bin

$(B):
	mkdir -p $(B)

$(B)/bootloader.elf: $(SRCS) $(DEPS) | $(B)
	$(GCC) $(CFLAGS) $(LDFLAGS) -o $@ $(SRCS)

$(B)/bootloader.bin: $(B)/bootloader.elf
	$(OBJCOPY) -O binary --gap-fill 0xFF $< $@

# Full-flash image: bootloader at 0x0, 0xFF-padded to the flash limit.
$(B)/firmware.bin: $(B)/bootloader.elf
	$(OBJCOPY) -O binary --gap-fill 0xFF --pad-to $(FLASH_SIZE) $< $@

# Same 0x55000 image under the name the top-level Makefile / flashing flow expect.
# (firmware.bin kept above so the sibling emulator that loads build/firmware.bin still works.)
$(B)/bootloader-flash.bin: $(B)/firmware.bin
	cp $< $@

# Implant-ready SRAM image: same micro.c/main.c, linked to run in place at the 0x1554 harness
# entry (0x20001800). No flash writes -> fully reversible. Load blob at 0x20001800, jump 0x20001801.
SRAM_SRCS := $(HERE)../link/stock_bootloader_sram/ram_entry.s $(HERE)micro.c $(HERE)main.c
$(B)/bootloader-sram.bin: $(SRAM_SRCS) $(HERE)../link/stock_bootloader_sram/bootloader-sram.ld $(wildcard $(HERE)../include/*.h) | $(B)
	$(GCC) $(CFLAGS) -DBL_SRAM_HIJACK -T$(HERE)../link/stock_bootloader_sram/bootloader-sram.ld -Wl,--build-id=none -o $(B)/bootloader-sram.elf $(SRAM_SRCS)
	@ivt=$$($(GCC:gcc=nm) $(B)/bootloader-sram.elf | awk '/ _sram_ivt$$/{print $$1}'); \
	 top=$$($(GCC:gcc=nm) $(B)/bootloader-sram.elf | awk '{print $$1}' | grep '^2000' | sort | tail -1); \
	 [ $$((0x$$ivt & 0x7f)) -eq 0 ] || { echo "FAIL: _sram_ivt $$ivt not 128-aligned"; exit 1; }; \
	 [ $$((0x$$top)) -lt $$((0x20002800)) ] || { echo "FAIL: image top 0x$$top hits stack 0x20002800"; exit 1; }; \
	 echo "sram layout OK: ivt=0x$$ivt top=0x$$top (stack 0x20002800)"
	$(OBJCOPY) -O binary $(B)/bootloader-sram.elf $(B)/bootloader-sram.bin
	rm -f $(B)/bootloader-sram.elf

# Host-side check of the flash-page overlay math (no hardware).
test:
	$(CC) -DBL_HOST_TEST -I$(HERE)../include $(HERE)main.c -o /tmp/bl_pagetest && /tmp/bl_pagetest
	@echo "build_page self-check OK"

clean:
	rm -rf $(B)

.INTERMEDIATE: $(B)/bootloader.elf
.PHONY: all bootloader.bin firmware.bin bootloader-flash.bin bootloader-sram.bin test clean
