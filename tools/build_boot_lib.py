#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build_boot_lib.py - compile boot-specific hc32f46_driver.lib (IS_RTOS2_SUPPORT=0)

流程：临时改 driverconfig.h 宏 -> UV4 编译 driver -> 拷贝为 hc32f46_boot.lib -> 恢复宏
boot 用 IS_RTOS2_SUPPORT=0 的 lib（不含 main.o，boot 自己提供 main）
"""
import os, re, sys, subprocess, shutil

SCAN = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # Scanner_20260901/
CFG  = os.path.join(SCAN, "hc32f46_driver", "trunk", "user", "inc", "driverconfig.h")
PROJ = os.path.join(SCAN, "hc32f46_driver", "trunk", "hc32f46_driver.uvprojx")
OUT  = os.path.join(SCAN, "hc32f46_driver", "trunk", "output", "hc32f46_driver.lib")
DEST = os.path.join(SCAN, "driver_lib", "hc32f46_boot.lib")
UV4  = os.environ.get("KEIL_UV4", r"D:\Keil_v5\UV4\UV4.exe")

def set_macros(rtos, icg):
    data = open(CFG, "rb").read()
    text = data.decode("utf-8", errors="surrogateescape")
    text = re.sub(r"#defines+IS_RTOS2_SUPPORTs+d", "#define IS_RTOS2_SUPPORT " + str(rtos), text)
    text = re.sub(r"#defines+ENABLE_ICG_TABLEs+d", "#define ENABLE_ICG_TABLE " + str(icg), text)
    open(CFG, "wb").write(text.encode("utf-8"))

def main():
    if not os.path.exists(UV4):
        sys.exit("UV4 not found: " + UV4)
    # 1) 切到 boot 配置
    set_macros(0, 1)
    print("[boot-lib] IS_RTOS2_SUPPORT=0, ENABLE_ICG_TABLE=1")
    # 2) 编译 driver
    log = os.path.join(SCAN, "_uv4_bootlib.log")
    rc = subprocess.run([UV4, "-r", PROJ, "-j0", "-o", log], cwd=os.path.dirname(PROJ),
                   capture_output=True, text=True)
    print("[boot-lib] UV4 rc=" + str(rc.returncode) + " log=" + log)
    t = open(log, encoding="utf-8", errors="replace").read() if os.path.exists(log) else ""
    if "0 Error(s)" not in t:
        # 恢复宏再退出
        set_macros(1, 0)
        sys.exit("[boot-lib] driver compile failed\n" + t[-800:])
    # 3) 拷贝为 boot lib
    if not os.path.exists(OUT):
        set_macros(1, 0)
        sys.exit("[boot-lib] output lib not found: " + OUT)
    shutil.copyfile(OUT, DEST)
    print("[boot-lib] saved -> " + DEST)
    # 4) 恢复 App 配置并重编 App lib（driver_lib/hc32f46_driver.lib 需含 main）
    set_macros(1, 0)
    log2 = os.path.join(SCAN, "_uv4_applib.log")
    subprocess.run([UV4, "-r", PROJ, "-j0", "-o", log2], cwd=os.path.dirname(PROJ),
                   capture_output=True, text=True)
    t2 = open(log2, encoding="utf-8", errors="replace").read() if os.path.exists(log2) else ""
    if "0 Error(s)" not in t2:
        sys.exit("[boot-lib] app-lib rebuild failed\n" + t2[-800:])
    print("[boot-lib] macros restored + app lib rebuilt (IS_RTOS2_SUPPORT=1)")

if __name__ == "__main__":
    main()