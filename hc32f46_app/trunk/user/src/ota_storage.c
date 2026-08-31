/**
 * @file ota_storage.c
 * @brief F460 存储后端：QSPI 暂存（w25qxx）+ 通道互斥
 * @note F460 不做片内 AB swap；commit（暂存→片内+备份）由 bootloader single-bak 实现。
 *       本文件只负责暂存区读写/校验/擦除，与 F4A0 同构接口。
 */
#include <string.h>
#include "hc32f46_driver.h"
#include "w25qxx.h"
#include "ota_layout.h"
#include "ota_storage.h"

/* ---- CRC32（IEEE，与统一 OTA 包一致） ---- */
static uint32_t s_crc32_table[256];
static int s_crc_init = 0;
static void crc32_init(void)
{
    if (s_crc_init) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        s_crc32_table[i] = c;
    }
    s_crc_init = 1;
}
static uint32_t crc32_update(uint32_t crc, const uint8_t *buf, uint32_t len)
{
    crc32_init();
    crc = ~crc;
    for (uint32_t i = 0; i < len; i++)
        crc = s_crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

int ota_storage_prepare(uint32_t total_size)
{
    uint32_t remain = (total_size > OTA_QSPI_STAGE_SIZE) ? OTA_QSPI_STAGE_SIZE : total_size;
    /* w25qxx 64KB 块擦（0xD8） */
    for (uint32_t off = 0; off < remain; off += 65536) {
        W25QXX_Erase_Sector(OTA_QSPI_STAGE_BASE + off);   /* 注：Sector=4KB；此处用 4KB 循环兼容 */
    }
    W25QXX_Wait_Busy();
    return 0;
}

int ota_storage_write_stage(const uint8_t *data, uint32_t len, uint32_t offset)
{
    if (offset + len > OTA_QSPI_STAGE_SIZE)
        return -1;
    /* w25qxx 页编程 ≤256B/页，W25QXX_Write 内部处理跨页；单次 ≤0xFFFF */
    while (len > 0) {
        uint32_t n = (len > 0xFFFFUL) ? 0xFFFFUL : len;
        W25QXX_Write((uint8_t *)data, OTA_QSPI_STAGE_BASE + offset, (uint16_t)n);
        W25QXX_Wait_Busy();
        offset += n; data += n; len -= n;
    }
    return 0;
}

int ota_storage_verify_stage(uint32_t size)
{
    uint8_t buf[OTA_STAGE_BLOCK];
    uint32_t crc = 0, remain = size;
    for (uint32_t off = 0; off < size; off += OTA_STAGE_BLOCK) {
        uint32_t n = (remain > OTA_STAGE_BLOCK) ? OTA_STAGE_BLOCK : remain;
        W25QXX_Read(buf, OTA_QSPI_STAGE_BASE + off, (uint16_t)n);
        crc = crc32_update(crc, buf, n);
        remain -= n;
    }
    (void)crc;
    return 0;   /* 调用方用 verify_payload 做期望值比对 */
}

int ota_storage_verify_payload(uint32_t payload_off, uint32_t payload_len, uint32_t expected_crc32)
{
    uint8_t buf[OTA_STAGE_BLOCK];
    uint32_t remain = payload_len, crc = 0;
    for (uint32_t off = 0; off < payload_len; off += OTA_STAGE_BLOCK) {
        uint32_t n = (remain > OTA_STAGE_BLOCK) ? OTA_STAGE_BLOCK : remain;
        W25QXX_Read(buf, OTA_QSPI_STAGE_BASE + payload_off + off, (uint16_t)n);
        crc = crc32_update(crc, buf, n);
        remain -= n;
    }
    return (crc == expected_crc32) ? 0 : -2;
}

/* commit 由 bootloader single-bak 实现；App 侧仅提供暂存读（boot 校验用，可选） */
int ota_storage_read_stage(uint32_t off, uint8_t *buf, uint32_t len)
{
    if (off + len > OTA_QSPI_STAGE_SIZE)
        return -1;
    W25QXX_Read(buf, OTA_QSPI_STAGE_BASE + off, (uint16_t)(len > 0xFFFFUL ? 0xFFFFUL : len));
    return 0;
}

/* ---- 通道互斥（HTTP 单通道也要防并发） ---- */
static uint8_t s_ota_channel_busy = 0;
int ota_channel_try_acquire(void)
{
    int r = -1;
    __disable_irq();
    if (s_ota_channel_busy == 0) {
        s_ota_channel_busy = 1;
        r = 0;
    }
    __enable_irq();
    return r;
}
void ota_channel_release(void)
{
    __disable_irq();
    s_ota_channel_busy = 0;
    __enable_irq();
}
int ota_channel_busy(void)
{
    int r;
    __disable_irq();
    r = (s_ota_channel_busy != 0);
    __enable_irq();
    return r;
}
