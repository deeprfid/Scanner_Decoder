/**
 *******************************************************************************
 * @file  boot_ota.h
 * @brief F460 Bootloader OTA 布局与常量（single-bank：QSPI 暂存 -> 片内 0x10000）
 *
 * 片内 Flash 512KB (0x00000000-0x0007FFFF)：
 *   Bootloader 0x00000000-0x0000FFFF (64KB)
 *   App        0x00010000-0x0007FFFF (448KB, 链接基址 0x10000)
 *
 * QSPI W25Q64 (8MB, XIP 窗口 0x98000000)：
 *   暂存区 0x00600000-0x006FFFFF (1MB)  新版固件 .otapkg（含 82B OTA1 包头）
 *
 * 引导决策（每次上电）：
 *   暂存区 magic=="OTA1" 且 CRC32 通过 -> commit 到片内 0x10000 -> 失效暂存 -> 跳 App
 *   否则 -> App@0x10000 有效则跳转，无效则死循环（等外部复位/烧录）
 *
 * 注意：commit（片内 Flash 擦/写）期间 Flash 阵列忙，执行代码必须来自 RAM ——
 *       boot_ota.o / flash.o / hc32_ll_efm.o 由 scatter 放到 RAM 执行区
 *       （MDK/config/linker/HC32F460xE.sct 的 RW_RAMCODE 0x20018000）。
 *******************************************************************************
 */
#ifndef __BOOT_OTA_H__
#define __BOOT_OTA_H__

/* ---- 片内布局 ---- */
#define BOOT_APP_BASE            0x00010000UL   /* App 链接基址 */
#define BOOT_APP_MAX_SIZE        0x00070000UL   /* App 区上限 448KB (0x10000-0x7FFFF) */

/* ---- QSPI 暂存（W25Q64 8MB，尾部 1MB） ---- */
#define BOOT_QSPI_STAGE_BASE     0x00600000UL
#define BOOT_QSPI_STAGE_MAX      0x00100000UL   /* 1MB */

/* ---- 统一 OTA1 包头（与 tools/ota_pack.py 一致） ---- */
#define BOOT_OTA_HDR_LEN         82UL
#define BOOT_OTA_MAGIC0          'O'
#define BOOT_OTA_MAGIC1          'T'
#define BOOT_OTA_MAGIC2          'A'
#define BOOT_OTA_MAGIC3          '1'
#define BOOT_OTA_VER_OFF         4UL
#define BOOT_OTA_LEN_OFF         10UL
#define BOOT_OTA_CRC_OFF         14UL

/* ---- App 向量表有效性判定（栈顶在 SRAM 内；F460 SRAM 0x1FFF8000 起 188KB） ---- */
#define BOOT_SRAM_BASE           0x1FFF8000UL
#define BOOT_SRAM_TOP            0x20027000UL   /* 0x1FFF8000 + 0x2F000 */

/*******************************************************************************
 * Function prototypes
 ******************************************************************************/
void BOOT_OTA_Run(void);   /* 不返回：commit / 跳 App / 死循环 */

#endif /* __BOOT_OTA_H__ */
