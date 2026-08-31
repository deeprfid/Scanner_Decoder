/**
 * @file ota_storage.h
 * @brief F460 升级存储后端（QSPI 暂存 w25qxx + 通道互斥；commit 归 boot single-bak）
 */
#ifndef OTA_STORAGE_H
#define OTA_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "ota_layout.h"

#define OTA_STAGE_BLOCK  1024   /* 块大小（QSPI<->片内搬运） */

/* 存储后端（供状态机/Agent 调用） */
int ota_storage_prepare(uint32_t total_size);
int ota_storage_write_stage(const uint8_t *data, uint32_t len, uint32_t offset);
int ota_storage_verify_stage(uint32_t size);
int ota_storage_verify_payload(uint32_t payload_off, uint32_t payload_len, uint32_t expected_crc32);
int ota_storage_read_stage(uint32_t off, uint8_t *buf, uint32_t len);

/* 通道互斥 */
int  ota_channel_try_acquire(void);
void ota_channel_release(void);
int  ota_channel_busy(void);

#endif /* OTA_STORAGE_H */
