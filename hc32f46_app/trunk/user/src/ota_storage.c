/**
 * @file ota_storage.c
 * @brief F460 瀛樺偍鍚庣锛歈SPI 鏆傚瓨锛坵25qxx锛�+ 閫氶亾浜掓枼
 * @note F460 涓嶅仛鐗囧唴 AB swap锛沜ommit锛堟殏瀛樷啋鐗囧唴+澶囦唤锛夌敱 bootloader single-bak 瀹炵幇銆�
 *       鏈枃浠跺彧璐熻矗鏆傚瓨鍖鸿鍐�/鏍￠獙/鎿﹂櫎锛屼笌 F4A0 鍚屾瀯鎺ュ彛銆�
 */
#include <string.h>
#include "hc32f46_driver.h"
#include "w25qxx.h"
#include "ota_layout.h"
#include "ota_storage.h"

/* ---- CRC32锛圛EEE锛屼笌缁熶竴 OTA 鍖呬竴鑷达級 ---- */
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
    /* w25qxx 64KB 鍧楁摝锛�0xD8锛� */
    for (uint32_t off = 0; off < remain; off += 65536) {
        W25QXX_Erase_Sector((OTA_QSPI_STAGE_BASE + off) / 4096UL);   /* 扇区索引 */   /* 娉細Sector=4KB锛涙澶勭敤 4KB 寰幆鍏煎 */
    }
    W25QXX_Wait_Busy();
    return 0;
}

int ota_storage_write_stage(const uint8_t *data, uint32_t len, uint32_t offset)
{
    if (offset + len > OTA_QSPI_STAGE_SIZE)
        return -1;
    /* w25qxx 椤电紪绋� 鈮�256B/椤碉紝W25QXX_Write 鍐呴儴澶勭悊璺ㄩ〉锛涘崟娆� 鈮�0xFFFF */
    while (len > 0) {
        uint32_t n = (len > 0xFFFFUL) ? 0xFFFFUL : len;
        /* 暂存已整区预擦除(FF)：直接用纯页编程，跳过 W25QXX_Write 的整扇区回读(提速) */
        W25QXX_Write_NoCheck((uint8_t *)data, OTA_QSPI_STAGE_BASE + offset, (uint16_t)n);
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
    return 0;   /* 璋冪敤鏂圭敤 verify_payload 鍋氭湡鏈涘�兼瘮瀵� */
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

/* commit 鐢� bootloader single-bak 瀹炵幇锛汚pp 渚т粎鎻愪緵鏆傚瓨璇伙紙boot 鏍￠獙鐢紝鍙�夛級 */
int ota_storage_read_stage(uint32_t off, uint8_t *buf, uint32_t len)
{
    if (off + len > OTA_QSPI_STAGE_SIZE)
        return -1;
    W25QXX_Read(buf, OTA_QSPI_STAGE_BASE + off, (uint16_t)(len > 0xFFFFUL ? 0xFFFFUL : len));
    return 0;
}

/* ---- 閫氶亾浜掓枼锛圚TTP 鍗曢�氶亾涔熻闃插苟鍙戯級 ---- */
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
