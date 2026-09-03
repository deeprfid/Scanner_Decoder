/**
 *******************************************************************************
 * @file  boot_ota.h
 * @brief F460 Bootloader OTA 布局与常量（single-bak：QSPI 暂存 + 备份 + 状态区）
 *
 * 片内 Flash 512KB：
 *   Bootloader 0x00000000-0x0000FFFF (64KB)
 *   App        0x00010000-0x0007FFFF (448KB, 链接基址 0x10000)
 *
 * QSPI W25Q64 (8MB，与 App ota_layout.h 完全一致)：
 *   暂存区 0x000000-0x07FFFF (512KB)   新固件 .otapkg（82B OTA1 头 + payload）
 *   备份区 0x080000-0x0FFFFF (512KB)   升级前旧固件备份（回滚用）
 *   状态区 0x100000 (4KB 扇区)         OTA 状态记录（24B，magic+crc）
 *
 * 引导决策（照 git 既定 ota_boot_single 语义）：
 *   状态 NEED_COMMIT   -> 校验暂存 -> 片内旧固件备份到 QSPI 备份区 -> 暂存写片内
 *                        -> 状态置 NEED_CONFIRM -> 跳新固件自检
 *   状态 NEED_CONFIRM  -> boot_count++；超限 -> 从备份区恢复旧固件 -> 清状态 -> 跳 App
 *   无有效状态/无动作位 -> 正常跳 App
 *******************************************************************************
 */
#ifndef __BOOT_OTA_H__
#define __BOOT_OTA_H__

/* ---- 片内布局 ---- */
#define BOOT_APP_BASE            0x00010000UL   /* App 链接基址 */
#define BOOT_APP_MAX_SIZE        0x00070000UL   /* App 区上限 448KB (0x10000-0x7FFFF) */

/* ---- QSPI W25Q64 8MB 分区（与 App ota_layout.h 一致） ---- */
#define BOOT_QSPI_STAGE_BASE     0x00000000UL
#define BOOT_QSPI_STAGE_SIZE     0x00080000UL   /* 512KB */
#define BOOT_QSPI_BACKUP_BASE    0x00080000UL
#define BOOT_QSPI_BACKUP_SIZE    0x00080000UL   /* 512KB */
#define BOOT_QSPI_STATE_BASE     0x00100000UL   /* 4KB 扇区 */
#define BOOT_QSPI_STATE_SIZE     0x00001000UL

/* ---- 统一 OTA1 包头（与 tools/ota_pack.py / App 一致） ---- */
#define BOOT_OTA_HDR_LEN         82UL
#define BOOT_OTA_MAGIC0          'O'
#define BOOT_OTA_MAGIC1          'T'
#define BOOT_OTA_MAGIC2          'A'
#define BOOT_OTA_MAGIC3          '1'
#define BOOT_OTA_VER_OFF         4UL
#define BOOT_OTA_LEN_OFF         10UL
#define BOOT_OTA_CRC_OFF         14UL

/* ---- OTA 状态记录（32B，与 App ota_state_rec_t 完全一致，CRC 覆盖 [0,28)） ---- */
#define BOOT_FLAG_MAGIC          0x4F544131UL   /* "OTA1" LE */
#define BOOT_FLAG_NEED_COMMIT    (1UL << 0)
#define BOOT_FLAG_NEED_CONFIRM   (1UL << 1)
#define BOOT_MAX_BOOT_COUNT      3UL
#define BOOT_STATE_CRC_LEN       28UL

typedef struct {
    uint32_t magic;       /* BOOT_FLAG_MAGIC */
    uint32_t flags;       /* NEED_COMMIT | NEED_CONFIRM */
    uint32_t size;        /* 新固件 payload 长度 */
    uint32_t version;     /* 新版本 */
    uint32_t boot_count;  /* 新固件启动计数 */
    uint32_t progress;    /* 下载进度（boot 置 0） */
    uint32_t result;      /* 0=ok 1=fail 2=rollback（boot 置 0） */
    uint32_t crc32;       /* 覆盖 [0,28) */
} boot_state_t;

/* ---- App 向量表有效性判定（栈顶在 SRAM 内；F460 SRAM 0x1FFF8000 起 188KB） ---- */
#define BOOT_SRAM_BASE           0x1FFF8000UL
#define BOOT_SRAM_TOP            0x20027000UL

/*******************************************************************************
 * Function prototypes
 ******************************************************************************/
void BOOT_OTA_Run(void);   /* 不返回：commit / 回滚 / 跳 App / 死循环 */

#endif /* __BOOT_OTA_H__ */
