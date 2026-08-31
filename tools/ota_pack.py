#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ota_pack.py — 统一 OTA 包打包工具（bin/Intel-HEX -> .otapkg）

包格式（与固件端 ota_pkg / parse_pkg_hdr 一致）：
  [0:4]   MAGIC "OTA1"
  [4:8]   version  (uint32 LE)
  [8]     platform_id (1=HC32F460, 2=HC32F4A0, 3=RK3506G, 4=RK3566)
  [9]     app_id   (0=主固件)
  [10:14] payload_len (uint32 LE)
  [14:18] CRC32(payload) (uint32 LE, IEEE 0xEDB88320)
  [18:50] SHA256(payload)
  [50:82] HMAC-SHA256(demo_key, [0:50]+payload)   # Phase 3 设备端暂不验签
  [82:]   payload（MCU=固件 bin）

用法：
  python ota_pack.py fw.hex --version 0x01020000 --platform 2 --app 0 -o fw.otapkg
  python ota_pack.py fw.bin --version 0x01020000 --platform 2 -o fw.otapkg
"""
import argparse, binascii, hashlib, hmac, struct, sys, zlib
import sys
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")


OTA_MAGIC = b"OTA1"
OTA_HEADER_LEN = 82
OTA_SIGN_OFF = 50        # HMAC 覆盖起点（[0:50]+payload）
DEMO_HMAC_KEY = b"HC32F4A0_OTA_KEY_DEMO_20260814"

def parse_hex(path):
    """Intel HEX -> (start_addr, bytes) 连续 bin（空隙补 0xFF）"""
    data = bytearray()
    base = 0
    addr = None
    start = 0
    for ln, line in enumerate(open(path, "r"), 1):
        line = line.strip()
        if not line or line[0] != ':':
            continue
        b = binascii.unhexlify(line[1:])
        if len(b) < 5:
            raise SystemExit(f"{path}:{ln}: bad hex record")
        reclen, recaddr, rectype = b[0], struct.unpack(">H", b[1:3])[0], b[3]
        if rectype == 0x04:          # extended linear address
            base = struct.unpack(">H", b[4:6])[0] << 16
            continue
        if rectype == 0x01:          # EOF
            break
        if rectype != 0x00:
            continue
        if reclen + 5 != len(b):
            raise SystemExit(f"{path}:{ln}: bad length")
        a = base + recaddr
        if addr is None:
            addr = a
            start = a
        if a > addr + len(data):     # gap -> fill 0xFF
            data.extend(b"\xff" * (a - (addr + len(data))))
        data.extend(b[4:4 + reclen])
        addr = a
    if addr is None:
        raise SystemExit(f"{path}: no data records")
    return start, bytes(data)

def build_pkg(payload: bytes, version: int, platform: int, app_id: int, key: bytes) -> bytes:
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    sha = hashlib.sha256(payload).digest()
    hdr = OTA_MAGIC + struct.pack("<I", version) + bytes([platform, app_id]) \
        + struct.pack("<I", len(payload)) + struct.pack("<I", crc) + sha
    sig = hmac.new(key, hdr[:OTA_SIGN_OFF] + payload, hashlib.sha256).digest()   # 覆盖 [0:50]+payload
    return hdr + sig + payload

def main():
    ap = argparse.ArgumentParser(description="打包统一 OTA 包（.otapkg）")
    ap.add_argument("input", help="固件文件：.hex (Intel HEX) 或 .bin")
    ap.add_argument("--version", type=lambda x: int(x, 0), default=0x01020000, help="版本号，如 0x01020000")
    ap.add_argument("--platform", type=int, default=2, help="平台ID：1=F460 2=F4A0 3=RK3506G 4=RK3566")
    ap.add_argument("--app", type=int, default=0, help="app_id（0=主固件）")
    ap.add_argument("--key", default=DEMO_HMAC_KEY.decode(), help="HMAC 密钥字符串（demo 默认）")
    ap.add_argument("--key-file", default=None, help="32 字节密钥文件（hex 或二进制，优先于 --key）")
    ap.add_argument("-o", "--output", default="firmware.otapkg", help="输出 .otapkg 路径")
    a = ap.parse_args()

    if a.input.lower().endswith(".hex"):
        start, payload = parse_hex(a.input)
    else:
        payload = open(a.input, "rb").read()
        start = 0
    if len(payload) == 0:
        raise SystemExit("empty payload")
    key = a.key.encode()
    if a.key_file:
        with open(a.key_file, "rb") as kf:
            kb = kf.read().strip()
        try:
            key = bytes.fromhex(kb.decode().strip()) if len(kb) == 64 else kb
        except ValueError:
            key = kb
        if len(key) != 32:
            sys.exit(f"密钥文件必须 32 字节（当前 {len(key)}）")
    pkg = build_pkg(payload, a.version, a.platform, a.app, key)
    open(a.output, "wb").write(pkg)

    print(f"payload: {len(payload)} bytes (start 0x{start:X})")
    print(f"version: 0x{a.version:08X}  platform: {a.platform}  app_id: {a.app}")
    print(f"crc32:   0x{(zlib.crc32(payload) & 0xFFFFFFFF):08X}")
    print(f"sha256:  {hashlib.sha256(payload).hexdigest()}")
    print(f"pkg:     {len(pkg)} bytes  ->  {a.output}")

if __name__ == "__main__":
    main()
