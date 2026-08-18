import argparse
import sys

CHUNK = 0x80
PATCH_ADDR = 0x1554
TRIGGER = [0x00, 0x32, 0x01, 0x00, 0x01, 0x00]
ORIG_PATCH = bytes.fromhex("fff756fd5a4a136d5e498b43136500f0")


def fmt(bs):
    return " ".join(f"{b:02X}" for b in bs)


def jump_patch(target):
    return bytes([0x00, 0x48, 0x00, 0x47]) + (target | 1).to_bytes(4, "little") + bytes([0xC0, 0x46]) * 4


def inject_apdus(addr, payload):
    out = []
    base = None
    i = 0
    while i < len(payload):
        cur = addr + i
        cur_base = cur & 0xFFFF0000
        if cur_base != base:
            base = cur_base
            out.append([0x00, 0x50, (base >> 24) & 0xFF, (base >> 16) & 0xFF, 0x00])
        room = 0x10000 - (cur & 0xFFFF)
        n = min(CHUNK, len(payload) - i, room)
        off = cur - base
        out.append([0x00, 0x5C, (off >> 8) & 0xFF, off & 0xFF, n] + list(payload[i:i + n]))
        i += n
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("addr", type=lambda s: int(s, 16))
    ap.add_argument("file", type=argparse.FileType("rb"))
    ap.add_argument("--comment", default=None)
    ap.add_argument("--jump-to", type=lambda s: int(s, 0), default=None,
                    help="after loading, patch scrub_ram_quick (0x1554) to jump here and trigger")
    ap.add_argument("--restore-jump-to", action="store_true",
                    help="after --jump-to, restore the original scrub_ram_quick body "
                         "(skip if the jumped-to bootloader won't service stock 0x5X commands)")
    ap.add_argument("--skip-verify", action="store_true",
                    help="omit the patch-site read that asserts scrub_ram_quick holds its original "
                         "body -- use when the jump patch is already committed at 0x1554")
    args = ap.parse_args()

    payload = args.file.read()
    lines = []
    if args.comment:
        lines += ["#" + args.comment, ""]
    for frame in inject_apdus(args.addr, payload):
        lines += [">" + fmt(frame), "<90 00"]

    if args.jump_to is not None:
        off = PATCH_ADDR & 0xFFFF
        if not args.skip_verify:
            lines += ["", ":Verifying patch site holds original body", ""]
            lines += [">" + fmt([0x00, 0x50, 0x00, 0x00, 0x00]), "<90 00"]
            lines += [">" + fmt([0x00, 0x5A, (off >> 8) & 0xFF, off & 0xFF, len(ORIG_PATCH)]),
                      "<" + fmt(ORIG_PATCH) + " 90 00"]

        lines += ["", ":Patching scrub_ram_quick and running", ""]
        for frame in inject_apdus(PATCH_ADDR, jump_patch(args.jump_to)):
            lines += [">" + fmt(frame), "<90 00"]
        lines += [">" + fmt(TRIGGER)]
        if args.restore_jump_to:
            lines += ["<90 00", "", ":Restoring original scrub_ram_quick body", ""]
            for frame in inject_apdus(PATCH_ADDR, ORIG_PATCH):
                lines += [">" + fmt(frame), "<90 00"]

    sys.stdout.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
