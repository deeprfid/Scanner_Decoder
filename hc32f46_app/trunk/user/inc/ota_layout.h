/**
 * @file ota_layout.h
 * @brief F460 OTA 布局常量（QSPI 16MB + 片内 512KB），与 F4A0 同构命名
 *
 * QSPI 16MB 分区（已定稿）：
 *   暂存区 0x000000-0x07FFFF (512KB)  新固件
 *   备份区 0x080000-0x0FFFFF (512KB)  升级前旧固件备份
 *   FlashDB 0x100000-0x8FFFFF (8MB)  参数/配置
 *   网页/FS 0x900000-0xEFFFFF (6MB)  index.html / 标签 CSV
 *   备用    0xF00000-0xFFFFFF (1MB)  OTA 状态扇区等
 *
 * 片内 512KB：App 0x16000 起（boot 跳转地址）；标志区待定（App 区末端扇区）
 */
#ifndef OTA_LAYOUT_H
#define OTA_LAYOUT_H

#include <stdint.h>

/* ---- QSPI 分区 ---- */
#define OTA_QSPI_STAGE_BASE    0x00000000UL   /* 暂存区（新固件） */
#define OTA_QSPI_STAGE_SIZE    0x00080000UL   /* 512KB */
#define OTA_QSPI_BACKUP_BASE   0x00080000UL   /* 备份区（旧固件） */
#define OTA_QSPI_BACKUP_SIZE   0x00080000UL   /* 512KB */

/* 升级状态扇区（备用区头部，4KB 扇区对齐） */
#define OTA_STATE_SECTOR_BASE  0x00100000UL   /* ״̬��: ��������, W25Q64 8MB �� */
#define OTA_STATE_SECTOR_SIZE  0x00001000UL   /* 4KB */   /* 4KB */

/* ---- 片内 ---- */
#define OTA_APP_BASE           0x00016000UL   /* App 链接基址（boot run_app 跳转地址） */
#define OTA_APP_MAX_SIZE       0x0007A000UL   /* App 区上限（512KB-起始偏移，预留标志扇区） */

/* F460 固件版本（与打包 ota_pack.py --version 一致；F460 首个 OTA 版本） */
#define OTA_FW_VERSION          0x01000000UL

/* OTA 包头（与统一 OTA1 包一致） */
#define OTA_PKG_HDR_LEN        82UL
#define OTA_PKG_MAGIC0         'O'
#define OTA_PKG_MAGIC1         'T'
#define OTA_PKG_MAGIC2         'A'
#define OTA_PKG_MAGIC3         '1'
#define OTA_PKG_VER_OFF        4UL
#define OTA_PKG_LEN_OFF        10UL
#define OTA_PKG_CRC_OFF        14UL

#endif /* OTA_LAYOUT_H */
