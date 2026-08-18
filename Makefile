PYTHON   ?= python3
GCC      ?= arm-none-eabi-gcc
OBJCOPY  ?= arm-none-eabi-objcopy
PFLOADER ?= pf-loader
LDSCRIPT ?= bootloader_payload/flash_after_bl.ld

HERE := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
B    := $(HERE)build
EX   := $(B)/examples
SRCS := $(wildcard $(HERE)examples/*.c)
BINS := $(patsubst $(HERE)examples/%.c,$(EX)/%.bin,$(SRCS))
EX_PF := $(patsubst $(HERE)examples/%.c,$(EX)/%.pf,$(SRCS))

CC_M0  := -mcpu=cortex-m0 -mthumb -ffreestanding -fno-builtin -nostdlib -nostartfiles \
          -I$(HERE)include -Os -Wl,--build-id=none
CFLAGS := $(CC_M0) -T$(HERE)link/$(LDSCRIPT)

BL_BINS := bootloader.bin bootloader-flash.bin bootloader-sram.bin
PF      := $(B)/ivt_defuse.pf $(B)/bootloader.pf $(B)/bootloader-sram.pf

all: $(BINS) $(EX_PF) bootloader $(B)/ivt_defuse.bin $(PF)

bootloader:
	$(MAKE) -C $(HERE)bootloader $(BL_BINS)

$(B) $(EX):
	mkdir -p $@

$(EX)/%.bin: $(HERE)examples/%.c $(HERE)link/$(LDSCRIPT) $(wildcard $(HERE)include/*.h) | $(EX)
	$(GCC) $(CFLAGS) -o $(EX)/$*.elf $<
	$(OBJCOPY) -O binary $(EX)/$*.elf $(EX)/$*.bin
	rm -f $(EX)/$*.elf

# app .bin -> .pf: load to the app base 0x8000 (bootloader_payload link base, clear of stock code)
# via OUR bootloader's opcodes. NOT generate_pf.py -- that's the stock 0x5X bank, gone once BL is at 0x0.
$(EX)/%.pf: $(EX)/%.bin $(HERE)tools/generate_app_pf.py
	$(PYTHON) $(HERE)tools/generate_app_pf.py 0x8000 $< --comment "load $* at app base 0x8000" > $@

# ivt_defuse: single .c, XIP .text at flash 0x9000, entry _start (jumped to via the 0x1554 patch).
# reloc_ivt[] is .bss and gets written at runtime, so .bss must be RAM, not the 0x9000 flash.
# ponytail: 0x20002000 is a scratch base assumed free while the stock OS services the flash writes
# -- if the card faults during ivt_defuse, move it to RAM the stock OS provably isn't using.
$(B)/ivt_defuse.bin: $(HERE)tools/ivt_defuse/ivt_defuse.c $(wildcard $(HERE)include/*.h) | $(B)
	$(GCC) $(CC_M0) -Wl,-e_start -Wl,-Ttext=0x9000 -Wl,--section-start=.bss=0x20002000 \
	    -o $(B)/ivt_defuse.elf $<
	$(OBJCOPY) -O binary $(B)/ivt_defuse.elf $(B)/ivt_defuse.bin
	rm -f $(B)/ivt_defuse.elf

$(B)/ivt_defuse.pf: $(B)/ivt_defuse.bin $(HERE)tools/ivt_defuse/generate_pf.py $(HERE)tools/generate_pf.py
	$(PYTHON) $(HERE)tools/ivt_defuse/generate_pf.py > $@

$(B)/bootloader.pf: bootloader $(HERE)tools/generate_pf.py
	$(PYTHON) $(HERE)tools/generate_pf.py 0x0 $(B)/bootloader.bin \
	    --comment "install bootloader at flash 0x0 (run ivt_defuse.pf first)" > $@

$(B)/bootloader-sram.pf: bootloader $(HERE)tools/generate_pf.py
	$(PYTHON) $(HERE)tools/generate_pf.py 0x20001800 $(B)/bootloader-sram.bin \
	    --jump-to 0x20001801 --comment "hijack-load SRAM bootloader + run" --skip-verify > $@

flash_bootloader: $(B)/ivt_defuse.pf $(B)/bootloader.pf
	$(PFLOADER) -s $(B)/ivt_defuse.pf -s $(B)/bootloader.pf

sram_load_bootloader: $(B)/bootloader-sram.pf
	$(PFLOADER) -s $(B)/bootloader-sram.pf

test_bootloader:
	$(PFLOADER) -s $(HERE)tools/bl_test_commands.pf

selftest:
	$(GCC) -mcpu=cortex-m0 -mthumb -ffreestanding -I$(HERE)include -c $(HERE)include/selftest.c -o /dev/null
	@echo "selftest OK: struct offsets match confirmed hardware"

clean:
	rm -rf $(B)

fmt:
	uvx black $(HERE)tools

fmt-check:
	uvx black --check $(HERE)tools

.PHONY: all bootloader selftest clean fmt fmt-check \
        flash_bootloader sram_load_bootloader test_bootloader
