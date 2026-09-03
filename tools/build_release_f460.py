#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build_release_f460.py - F460 scanner one-click firmware release generation

Usage (after the KEIL projects are compiled):
    python tools/build_release_f460.py
    python tools/build_release_f460.py --build    (also compiles driver/boot/app via UV4)

Outputs to Scanner_20260901/release/ (timestamped):
    merged_f460_<ver>_<YYYYMMDD_HHMM>.bin    DAP-LINK/J-Link flash (boot@0x0(64KB) + app@0x10000)
    fw_<ver>_<YYYYMMDD_HHMM>.otapkg          HTTP channel upgrade package (platform=1=F460)
    FW.BIN                                   fixed-name binary (byte-identical to pkg)
    manifest.txt                             version/time/size/SHA256

Version source: OTA_FW_VERSION in hc32f46_app/trunk/user/inc/ota_layout.h
"""
import os, re, sys, hashlib, subprocess, shutil, datetime
SCAN = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # Scanner_20260901/
APP_BIN   = os.path.join(SCAN, "hc32f46_app", "trunk", "output", "Firmware.bin")   # 旧产物（不推荐用于打包，见 APP_HEX）
APP_HEX   = os.path.join(SCAN, "hc32f46_app", "trunk", "output", "Firmware.hex")   # App 当前链接基址 0x10000
VHDR      = os.path.join(SCAN, "hc32f46_app", "trunk", "user", "inc", "ota_layout.h")
RELEASE   = os.path.join(SCAN, "release")
TOOLS     = os.path.join(SCAN, "tools")                    # 本工程 tools（完全独立，不依赖主仓库）
MERGE_BIN = os.path.join(TOOLS, "merge_bin.py")
OTA_PACK  = os.path.join(TOOLS, "ota_pack.py")

# UV4 one-click build (set env KEIL_UV4 to override)
UV4 = os.environ.get("KEIL_UV4", r"D:\Keil_v5\UV4\UV4.exe")
PROJECTS = [
    ("driver", os.path.join(SCAN, "hc32f46_driver", "trunk", "hc32f46_driver.uvprojx")),
    ("boot",   os.path.join(SCAN, "boot_iap", "MDK", "iap_boot.uvprojx")),
    ("app",    os.path.join(SCAN, "hc32f46_app", "trunk", "firmware_t.uvprojx")),
]


def get_version():
    txt = open(VHDR, encoding="utf-8", errors="replace").read()
    m = re.search(r"#define\s+OTA_FW_VERSION\s+(0x[0-9A-Fa-f]+)", txt)
    if not m:
        sys.exit("OTA_FW_VERSION not found: " + VHDR)
    return int(m.group(1), 16)


def run(cmd):
    r = subprocess.run(cmd, cwd=SCAN, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("command failed: " + " ".join(cmd) + "\n" + r.stdout + r.stderr)
    return r.stdout


def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for blk in iter(lambda: f.read(65536), b""):
            h.update(blk)
    return h.hexdigest()


def pkg_version(p):
    b = open(p, "rb").read(8)
    if len(b) >= 8 and b[0:4] == b"OTA1":
        return int.from_bytes(b[4:8], "little")
    return None


def build_all():
    if not os.path.exists(UV4):
        sys.exit("UV4.exe not found: " + UV4 + " (set env KEIL_UV4)")
    for name, proj in PROJECTS:
        if not os.path.exists(proj):
            sys.exit("project missing: " + proj)
        log = os.path.join(SCAN, "_uv4_f460_" + name + ".log")
        print("[build] compiling " + name + " ...")
        # -r: full rebuild（避免增量缓存 main.o 重复链接问题）
        subprocess.run([UV4, "-r", proj, "-j0", "-o", log], cwd=os.path.dirname(proj),
                       capture_output=True, text=True)
        t = ""
        if os.path.exists(log):
            t = open(log, encoding="utf-8", errors="replace").read()
        if "0 Error(s)" not in t:
            sys.exit("[build] " + name + " failed\n" + t[-800:])
    print("[build] all 3 projects compiled (libs: driver=h32f46_driver.lib / boot=hc32f46_boot.lib fixed)")


def main():
    if "--build" in sys.argv:
        build_all()

    ver = get_version()
    verstr = "0x%08X" % ver
    print("=== F460 release generation  ver=" + verstr + " ===")

    # locate boot hex + app hex/bin
    boot_hex = None
    for cand in os.listdir(os.path.join(SCAN, "boot_iap", "MDK", "output", "debug")):
        if cand.endswith(".hex"):
            boot_hex = os.path.join(SCAN, "boot_iap", "MDK", "output", "debug", cand)
    if not boot_hex:
        sys.exit("boot_iap output .hex not found (compile boot first)")
    app_hex = os.path.join(SCAN, "hc32f46_app", "trunk", "output", "Firmware.hex")
    if not os.path.exists(app_hex):
        sys.exit("app output Firmware.hex not found (compile app first)")

    os.makedirs(RELEASE, exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M")
    merged = os.path.join(RELEASE, "merged_f460_" + verstr + "_" + stamp + ".bin")
    pkg    = os.path.join(RELEASE, "fw_" + verstr + "_" + stamp + ".otapkg")
    fwbin  = os.path.join(RELEASE, "FW.BIN")

    # 1) DAP-LINK merged bin (boot@0x0 64KB + app@0x10000, 两个 hex 合并)
    run(["python", MERGE_BIN, boot_hex, app_hex, "-o", merged])
    # 2) upgrade package (HTTP channel, platform=1=F460, payload 从最新 Firmware.hex 取，
    #    基址 0x10000 起即为 boot commit 写入片内 0x10000 的完整 App 镜像)
    run(["python", OTA_PACK, app_hex, "--version", verstr, "--platform", "1", "-o", pkg])
    # 3) FW.BIN fixed name
    shutil.copyfile(pkg, fwbin)
    # 3) FW.BIN fixed name
    shutil.copyfile(pkg, fwbin)

    # 4) verify + manifest
    lines = ["F460 firmware release manifest  ver=" + verstr + "  built=" + stamp, ""]
    lines.append("[LATEST] merged =" + os.path.basename(merged))
    lines.append("         pkg    =" + os.path.basename(pkg))
    lines.append("")
    ok = True
    pv = pkg_version(pkg)
    if pv != ver:
        ok = False
        lines.append("[FAIL] pkg header ver %#010x != expected %#010x" % (pv, ver))
    else:
        lines.append("[OK]   pkg header ver %#010x  platform=1(F460)" % ver)
    same = os.path.getsize(pkg) == os.path.getsize(fwbin) and open(pkg, "rb").read() == open(fwbin, "rb").read()
    lines.append("  [OK]   FW.BIN identical to otapkg" if same else "  [FAIL] FW.BIN differs")
    if not same:
        ok = False
    for f in sorted(os.listdir(RELEASE)):
        if f.startswith("merged_f460") or f.endswith(".otapkg") or f == "FW.BIN":
            p = os.path.join(RELEASE, f)
            lines.append("  " + f + "  size=" + str(os.path.getsize(p)) + "  sha256=" + sha256(p))
    lines.append("")
    open(os.path.join(RELEASE, "manifest.txt"), "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print("\n".join(lines))
    if not ok:
        sys.exit("!! verification failed")
    print("\nall generated -> " + RELEASE)


if __name__ == "__main__":
    main()