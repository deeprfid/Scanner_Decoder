#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
merge_bin.py — 直接合并多个 Intel HEX 为连续 .bin（空隙填 0xFF，不经过中间 hex）

用途：F4A0 dual-bank 布局生产烧录用（boot@0x0 + app@0x10000 → merged.bin）。
与 merge_hex.py 的区别：直接输出二进制，避免"hex→bin"二次转换环节
（2026-08-19 真机联调发现：hex 中间产物烧录后设备 UART1 RX 异常，直出 bin 正常）。

用法：
  python merge_bin.py boot.hex app.hex -o merged.bin
  python merge_bin.py --list boot.hex app.hex
"""
import argparse, binascii, hashlib, struct, sys


def ep():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def hex2segs(path):
    """Intel HEX -> {addr: bytearray} 分段字典"""
    segs = {}
    base = 0
    for ln, line in enumerate(open(path, "r"), 1):
        line = line.strip()
        if not line or line[0] != ':':
            continue
        b = binascii.unhexlify(line[1:])
        if len(b) < 5:
            raise SystemExit(f"{path}:{ln}: bad record")
        reclen, recaddr, rectype = b[0], struct.unpack(">H", b[1:3])[0], b[3]
        if rectype == 0x04:
            base = struct.unpack(">H", b[4:6])[0] << 16
            continue
        if rectype == 0x01:
            break
        if rectype != 0x00:
            continue
        a = base + recaddr
        segs.setdefault(a, bytearray()).extend(b[4:4 + reclen])
    if not segs:
        raise SystemExit(f"{path}: no data")
    return segs


def main():
    ep()
    ap = argparse.ArgumentParser(description="合并 Intel HEX 直接生成 .bin")
    ap.add_argument("inputs", nargs="+", help="输入 hex（按顺序，地址自动定位）")
    ap.add_argument("-o", "--output", default="merged.bin")
    ap.add_argument("--list", action="store_true", help="仅列出地址范围")
    a = ap.parse_args()

    all_segs = {}
    for f in a.inputs:
        segs = hex2segs(f)
        if a.list:
            amin, amax = min(segs), max(s + len(d) for s, d in segs.items()) - 1
            print(f"{f}: 0x{amin:X}-0x{amax:X} ({amax-amin+1} bytes)")
            continue
        for addr, data in segs.items():
            if addr in all_segs or any(addr < s + len(d) for s, d in all_segs.items()):
                raise SystemExit(f"地址重叠: {f} @0x{addr:X}")
            all_segs[addr] = data
    if a.list:
        return

    start = min(all_segs)
    end = max(addr + len(d) for addr, d in all_segs.items()) - 1
    buf = bytearray([0xFF]) * (end - start + 1)
    for addr, data in all_segs.items():
        buf[addr - start:addr - start + len(data)] = data

    with open(a.output, "wb") as f:
        f.write(buf)
    print(f"合并完成: {a.output} ({len(buf)} bytes, 范围 0x{start:X}-0x{end:X})")
    print(f"sha256: {hashlib.sha256(buf).hexdigest()}")
    # 向量表抽查（boot SP @0x0 / app SP @0x10000）
    for name, off in (("boot", 0), ("app", 0x10000)):
        if off + 4 <= len(buf):
            print(f"{name} SP @0x{off:X}: {buf[off:off+4].hex()}")


if __name__ == "__main__":
    main()
