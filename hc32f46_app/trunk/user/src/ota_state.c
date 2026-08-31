/**
 * @file ota_state.c
 * @brief F460 升级状态存储（QSPI 扇区直存，替代 FlashDB——少一个依赖，掉电安全）
 *
 * 布局：QSPI 备用区 0xF00000 4KB 扇区（OTA_STATE_SECTOR_BASE）
 * 结构 24B：magic + flags + size + version + boot_count + progress + result + crc32
 * 写策略：先擦扇区再写整结构（4KB 扇区对齐，掉电只会丢整块——靠 crc 判失效）
 */
#include <string.h>
#include "hc32f46_driver.h"   /* TRACE, __disable_irq */
#include "w25qxx.h"
#include "ota_layout.h"
#include "ota_state.h"

#define OTA_STATE_MAGIC         0x4F544131UL   /* "OTA1" LE */
#define OTA_STATE_FLAG_NEED_COMMIT   (1UL << 0)
#define OTA_STATE_FLAG_NEED_CONFIRM  (1UL << 1)

/* CRC32（IEEE，与统一 OTA 包一致） */
static uint32_t s_crc_tab[256];
static int s_crc_ok = 0;
static void crc_tab_init(void)
{
    if (s_crc_ok) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        s_crc_tab[i] = c;
    }
    s_crc_ok = 1;
}
static uint32_t crc32_update(uint32_t crc, const uint8_t *buf, uint32_t len)
{
    crc_tab_init();
    crc = ~crc;
    for (uint32_t i = 0; i < len; i++)
        crc = s_crc_tab[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

typedef struct {
    uint32_t magic;
    uint32_t flags;        /* NEED_COMMIT | NEED_CONFIRM */
    uint32_t size;         /* 新固件 payload 长度 */
    uint32_t version;
    uint32_t boot_count;
    uint32_t progress;     /* 下载进度 */
    uint32_t result;       /* 0=ok 1=fail 2=rollback */
    uint32_t crc32;        /* 覆盖 [0,28) */
} ota_state_rec_t;

static int rec_read(ota_state_rec_t *r)
{
    uint32_t crc;
    W25QXX_Read((uint8_t *)r, OTA_STATE_SECTOR_BASE, sizeof(*r));
    if (r->magic != OTA_STATE_MAGIC) return -1;
    crc = crc32_update(0, (const uint8_t *)r, 28);
    if (r->crc32 != crc) return -1;
    return 0;
}

static int rec_write(const ota_state_rec_t *r)
{
    ota_state_rec_t tmp = *r;
    tmp.crc32 = crc32_update(0, (const uint8_t *)r, 28);
    /* 先擦 4KB 扇区再写（页编程） */
    W25QXX_Erase_Sector(OTA_STATE_SECTOR_BASE);
    W25QXX_Wait_Busy();
    /* 24B 单次页编程 */
    W25QXX_Write((uint8_t *)&tmp, OTA_STATE_SECTOR_BASE, sizeof(tmp));
    W25QXX_Wait_Busy();
    return 0;
}

/* ---- ota_state.h 接口 ---- */
void ota_state_init(void)
{
    /* 无特殊初始化；首次读无效即 IDLE */
}

ota_state_t ota_state_run(void)
{
    ota_state_rec_t r;
    if (rec_read(&r) != 0) return OTA_STATE_IDLE;
    if (r.flags & OTA_STATE_FLAG_NEED_COMMIT) {
        if (r.size > 0) return OTA_STATE_READY;
        return OTA_STATE_DOWNLOADING;
    }
    return OTA_STATE_IDLE;
}

void ota_set_progress(uint32_t bytes)
{
    ota_state_rec_t r;
    if (rec_read(&r) != 0) { memset(&r, 0, sizeof(r)); r.magic = OTA_STATE_MAGIC; }
    r.progress = bytes;
    rec_write(&r);
}

uint32_t ota_get_progress(void)
{
    ota_state_rec_t r;
    if (rec_read(&r) != 0) return 0;
    return r.progress;
}

uint32_t ota_get_copy_size(void)
{
    ota_state_rec_t r;
    if (rec_read(&r) != 0) return 0;
    return r.size;
}

int ota_mark_ready(uint32_t size, uint32_t version)
{
    ota_state_rec_t r;
    memset(&r, 0, sizeof(r));
    r.magic    = OTA_STATE_MAGIC;
    r.flags    = OTA_STATE_FLAG_NEED_COMMIT;
    r.size     = size;
    r.version  = version;
    rec_write(&r);
    TRACE("ota: mark_ready size=%lu ver=0x%08X\n", (unsigned long)size, (unsigned)version);
    return 0;
}

void ota_self_check_ok(void)
{
    ota_state_rec_t r;
    if (rec_read(&r) != 0) return;
    r.flags = 0;
    r.boot_count = 0;
    r.size = 0;
    r.version = 0;
    r.progress = 0;
    r.result = 0;
    rec_write(&r);
}

void ota_boot_confirm(void)
{
    ota_state_rec_t r;
    if (rec_read(&r) != 0) return;
    r.flags = 0;
    r.boot_count = 0;
    r.size = 0;
    r.version = 0;
    r.progress = 0;
    r.result = 0;
    rec_write(&r);
    ota_boot_count_clear();
}

void ota_fail(uint8_t result)
{
    ota_state_rec_t r;
    if (rec_read(&r) != 0) { memset(&r, 0, sizeof(r)); r.magic = OTA_STATE_MAGIC; }
    r.result = result;
    if (result == 2) r.flags = 0;   /* rollback：清 need_commit */
    rec_write(&r);
}

uint32_t ota_boot_count_get(void)
{
    ota_state_rec_t r;
    if (rec_read(&r) != 0) return 0;
    return r.boot_count;
}

void ota_boot_count_inc(void)
{
    ota_state_rec_t r;
    if (rec_read(&r) != 0) { memset(&r, 0, sizeof(r)); r.magic = OTA_STATE_MAGIC; }
    r.boot_count++;
    rec_write(&r);
}

void ota_boot_count_clear(void)
{
    ota_state_rec_t r;
    if (rec_read(&r) != 0) return;
    r.boot_count = 0;
    rec_write(&r);
}
