import argparse
import sys

CLA, OP_PUSH, OP_SET_ADDR = 0x00, 0x01, 0x02
PAGE, CHUNK = 0x200, 0xFF  # 512-B flash page; PUSH's n is one byte


def frame(ins, data):
    body = bytes([CLA, ins, 0, 0, len(data)]) + data
    return [">" + " ".join(f"{b:02X}" for b in body), "<90 00"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("addr", type=lambda s: int(s, 0))
    ap.add_argument("file", type=argparse.FileType("rb"))
    ap.add_argument("--comment")
    args = ap.parse_args()
    assert args.addr % PAGE == 0, "app base must be page-aligned"

    payload = args.file.read()
    lines = ["#" + args.comment, ""] if args.comment else []
    lines += frame(OP_SET_ADDR, args.addr.to_bytes(4, "big"))
    for p in range(0, len(payload), PAGE):  # per page ...
        page = payload[p:p + PAGE]
        for c in range(0, len(page), CHUNK):  # ... in <=255-B PUSHes
            lines += frame(OP_PUSH, page[c:c + CHUNK])
    sys.stdout.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
