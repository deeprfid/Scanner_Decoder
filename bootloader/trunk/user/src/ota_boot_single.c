
/**
 * @file ota_boot_single.c
 * @brief F460 bootloader single-bak OTA commit/rollback（FWlib 实现）
 *
 * 片内 512KB 单份 App（0x16000 起），QSPI 双镜像：
 *   暂存区 0x000000 (512KB)  新固件（App 收包写入）
 *   备份区 0x080000 (512KB)  升级前旧固件备份
 *   状态区 0x100000 (4KB)    升级标志（magic+flags+size+version+boot_count+crc）
 *
 * 流程：
 *   NEED_COMMIT  -> 校验暂存 CRC -> 备份片内旧固件到 QSPI 备份区 -> 暂存写片内
 *                   -> NEED_CONFIRM -> run_app
 *   NEED_CONFIRM -> boot_count++；超限 -> 从备份区恢复片内 -> 清标志 -> run_app
 */
#include <string.h>
#include "hc32_ddl.h"         /* 先加载 CMSIS/__IO/寄存器定义（w25qxx.c 同模式） */
void boot_printf(const char *fmt, ...);   /* boot_init.c: UART2 printf */
#define TRACE boot_printf
#include "hc32f460_efm.h"
#include "fw_jump_helper.h"   /* run_app, gPageBuffer */
#include "ota_boot_single.h"

/* ---- 布局（与 App 侧 ota_layout.h 一致）---- */
#define BSL_APP_BASE         0x00016000UL
#define BSL_APP_MAX_SIZE     0x0007A000UL   /* 512KB 减起始偏移+标志预留 */
#define BSL_QSPI_STAGE_BASE  0x00000000UL
#define BSL_QSPI_BACKUP_BASE 0x00080000UL
#define BSL_STATE_BASE       0x00100000UL   /* W25Q64 8MB: 备份区后 4KB 扇区 */
#define BSL_OTA_HDR_LEN      82UL
#define BSL_MAGIC0           'O'
#define BSL_MAGIC1           'T'
#define BSL_MAGIC2           'A'
#define BSL_MAGIC3           '1'
#define BSL_VER_OFF          4UL
#define BSL_LEN_OFF          10UL
#define BSL_CRC_OFF          14UL
#define BSL_FLAG_MAGIC       0x4F544131UL   /* "OTA1" LE */
#define BSL_FLAG_NEED_COMMIT   (1UL << 0)
#define BSL_FLAG_NEED_CONFIRM  (1UL << 1)
#define BSL_MAX_BOOT_COUNT     3UL

typedef struct {
    uint32_t magic;
    uint32_t flags;
    uint32_t size;
    uint32_t version;
    uint32_t boot_count;
    uint32_t crc32;
} bsl_flag_t;

/* ---- CRC32 ---- */
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

/* ---- QSPI 读/写（复用 App 同源 w25qxx.c，W25QXX_* API）---- */
#include "w25qxx.h"

static int bsl_qspi_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    while (len > 0) {
        uint32_t n = (len > 0xFFFFUL) ? 0xFFFFUL : len;
        W25QXX_Read(buf, addr, (uint16_t)n);
        addr += n; buf += n; len -= n;
    }
    return 0;
}
static int bsl_qspi_erase(uint32_t addr, uint32_t len)
{
    for (uint32_t a = addr; a < addr + len; a += 4096UL)
        W25QXX_Erase_Sector(a);
    W25QXX_Wait_Busy();
    return 0;
}
static int bsl_qspi_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    while (len > 0) {
        uint32_t n = (len > 0xFFFFUL) ? 0xFFFFUL : len;
        W25QXX_Write((uint8_t *)buf, addr, (uint16_t)n);
        W25QXX_Wait_Busy();
        addr += n; buf += n; len -= n;
    }
    return 0;
}

/* ---- 片内 EFM 读/写 ---- */
static uint32_t efm_read32(uint32_t addr)
{
    return *((volatile uint32_t *)addr);
}
static int efm_sector_erase(uint32_t addr)
{
    return (EFM_SectorErase(addr) == Ok) ? 0 : -1;
}
static int efm_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    /* 4 字节对齐写 */
    uint32_t pos = 0;
    while (pos + 4 <= len) {
        uint32_t w = (uint32_t)buf[pos] | ((uint32_t)buf[pos+1] << 8) |
                     ((uint32_t)buf[pos+2] << 16) | ((uint32_t)buf[pos+3] << 24);
        if (EFM_SingleProgram(addr + pos, w) != Ok)
            return -1;
        pos += 4;
    }
    if (pos < len) {   /* 尾部不足 4B：读-改-写 */
        uint32_t w = efm_read32(addr + pos);
        for (uint32_t i = pos; i < len; i++)
            ((uint8_t *)&w)[i - pos] = buf[i];
        if (EFM_SingleProgram(addr + pos, w) != Ok)
            return -1;
    }
    return 0;
}

/* ---- 标志 ---- */
static int flag_read(bsl_flag_t *f)
{
    uint32_t crc;
    if (f == NULL) return -1;
    bsl_qspi_read(BSL_STATE_BASE, (uint8_t *)f, sizeof(*f));
    if (f->magic != BSL_FLAG_MAGIC) return -1;
    crc = crc32_update(0, (const uint8_t *)f, 20);
    if (f->crc32 != crc) return -1;
    return 0;
}
static int flag_write(const bsl_flag_t *f)
{
    bsl_flag_t tmp = *f;
    tmp.crc32 = crc32_update(0, (const uint8_t *)f, 20);
    bsl_qspi_erase(BSL_STATE_BASE, 4096UL);
    bsl_qspi_write(BSL_STATE_BASE, (const uint8_t *)&tmp, sizeof(tmp));
    return 0;
}

/* ---- 暂存包校验（magic + CRC）---- */
static int verify_staged(uint32_t *fw_len, uint32_t *version)
{
    uint8_t hdr[BSL_OTA_HDR_LEN];
    uint32_t size, crc_expect, crc = 0, remain;

    bsl_qspi_read(BSL_QSPI_STAGE_BASE, hdr, sizeof(hdr));
    if (hdr[0] != BSL_MAGIC0 || hdr[1] != BSL_MAGIC1 ||
        hdr[2] != BSL_MAGIC2 || hdr[3] != BSL_MAGIC3)
        return -1;

    *version = ((uint32_t)hdr[BSL_VER_OFF]) | ((uint32_t)hdr[BSL_VER_OFF+1] << 8) |
               ((uint32_t)hdr[BSL_VER_OFF+2] << 16) | ((uint32_t)hdr[BSL_VER_OFF+3] << 24);
    size = ((uint32_t)hdr[BSL_LEN_OFF]) | ((uint32_t)hdr[BSL_LEN_OFF+1] << 8) |
           ((uint32_t)hdr[BSL_LEN_OFF+2] << 16) | ((uint32_t)hdr[BSL_LEN_OFF+3] << 24);
    crc_expect = ((uint32_t)hdr[BSL_CRC_OFF]) | ((uint32_t)hdr[BSL_CRC_OFF+1] << 8) |
                 ((uint32_t)hdr[BSL_CRC_OFF+2] << 16) | ((uint32_t)hdr[BSL_CRC_OFF+3] << 24);
    if (size == 0 || size > BSL_APP_MAX_SIZE)
        return -1;
    *fw_len = size;

    remain = size;
    for (uint32_t off = BSL_OTA_HDR_LEN; off < BSL_OTA_HDR_LEN + size; off += sizeof(gPageBuffer)) {
        uint32_t n = (remain > sizeof(gPageBuffer)) ? sizeof(gPageBuffer) : remain;
        bsl_qspi_read(BSL_QSPI_STAGE_BASE + off, (uint8_t *)gPageBuffer, n);
        crc = crc32_update(crc, (const uint8_t *)gPageBuffer, n);
        remain -= n;
    }
    return (crc == crc_expect) ? 0 : -2;
}

/* ---- commit：备份片内 -> 覆盖写片内 ---- */
static int commit_new_fw(uint32_t fw_len, uint32_t version)
{
    uint32_t remain;
    bsl_flag_t f;

    /* 1) 备份片内当前 App（0x16000 起 fw_len 字节）到 QSPI 备份区 */
    /* 备份区 512KB 整块擦（对齐 4KB） */
    bsl_qspi_erase(BSL_QSPI_BACKUP_BASE, 0x00080000UL);
    remain = fw_len;
    for (uint32_t off = 0; off < fw_len; off += sizeof(gPageBuffer)) {
        uint32_t n = (remain > sizeof(gPageBuffer)) ? sizeof(gPageBuffer) : remain;
        memcpy((uint8_t *)gPageBuffer, (const void *)(BSL_APP_BASE + off), n);
        bsl_qspi_write(BSL_QSPI_BACKUP_BASE + off, (const uint8_t *)gPageBuffer, n);
        remain -= n;
    }

    /* 2) 暂存（82B 头后）-> 片内 App 区（8KB 扇区擦 + 4B 写） */
    EFM_Unlock();
    EFM_FlashCmd(Enable);
    remain = fw_len;
    for (uint32_t off = 0; off < fw_len; off += sizeof(gPageBuffer)) {
        uint32_t n = (remain > sizeof(gPageBuffer)) ? sizeof(gPageBuffer) : remain;
        if ((off & 0x1FFFUL) == 0UL) {
            if (efm_sector_erase(BSL_APP_BASE + off) != 0) {
                EFM_FlashCmd(Disable);
                return -1;
            }
        }
        bsl_qspi_read(BSL_QSPI_STAGE_BASE + BSL_OTA_HDR_LEN + off, (uint8_t *)gPageBuffer, n);
        if (efm_write(BSL_APP_BASE + off, (const uint8_t *)gPageBuffer, n) != 0) {
            EFM_FlashCmd(Disable);
            return -1;
        }
        remain -= n;
    }
    EFM_FlashCmd(Disable);

    /* 3) 置 NEED_CONFIRM（新固件待自检） */
    memset(&f, 0, sizeof(f));
    f.magic = BSL_FLAG_MAGIC;
    f.flags = BSL_FLAG_NEED_CONFIRM;
    f.size  = fw_len;
    f.version = version;
    f.boot_count = 0;
    flag_write(&f);
    return 0;
}

/* ---- 回滚：从备份区恢复片内 ---- */
static int rollback(uint32_t fw_len)
{
    uint32_t remain = fw_len;
    bsl_flag_t f;

    EFM_Unlock();
    EFM_FlashCmd(Enable);
    for (uint32_t off = 0; off < fw_len; off += sizeof(gPageBuffer)) {
        uint32_t n = (remain > sizeof(gPageBuffer)) ? sizeof(gPageBuffer) : remain;
        if ((off & 0x1FFFUL) == 0UL) {
            if (efm_sector_erase(BSL_APP_BASE + off) != 0) {
                EFM_FlashCmd(Disable);
                return -1;
            }
        }
        bsl_qspi_read(BSL_QSPI_BACKUP_BASE + off, (uint8_t *)gPageBuffer, n);
        if (efm_write(BSL_APP_BASE + off, (const uint8_t *)gPageBuffer, n) != 0) {
            EFM_FlashCmd(Disable);
            return -1;
        }
        remain -= n;
    }
    EFM_FlashCmd(Disable);

    /* 清标志 */
    memset(&f, 0, sizeof(f));
    f.magic = BSL_FLAG_MAGIC;
    flag_write(&f);
    return 0;
}

/* ---- 主入口：返回 1=已处理（调用方应 run_app/复位），0=无 OTA 需处理 ---- */
int ota_boot_single_run(void)
{
    bsl_flag_t f;
    uint32_t fw_len = 0, version = 0;

    if (flag_read(&f) != 0)
        return 0;   /* 无 OTA 标志：走原有引导 */

    if (f.flags & BSL_FLAG_NEED_COMMIT) {
        if (verify_staged(&fw_len, &version) != 0) {
            TRACE("BOOT: staged verify FAIL, clear flag\n");
            memset(&f, 0, sizeof(f));
            f.magic = BSL_FLAG_MAGIC;
            flag_write(&f);
            return 0;   /* 校验失败：放弃升级，走旧固件 */
        }
        if (commit_new_fw(fw_len, version) != 0) {
            TRACE("BOOT: commit FAIL\n");
            return 0;
        }
        TRACE("BOOT: commit ok, run new fw\n");
        run_app(BSL_APP_BASE);   /* 新固件自检；失败会复位回来（NEED_CONFIRM） */
        return 1;
    }

    if (f.flags & BSL_FLAG_NEED_CONFIRM) {
        f.boot_count++;
        if (f.boot_count > BSL_MAX_BOOT_COUNT) {
            TRACE("BOOT: boot count %lu exceeded, rollback\n", (unsigned long)f.boot_count);
            rollback(f.size);
            run_app(BSL_APP_BASE);
            return 1;
        }
        flag_write(&f);
        TRACE("BOOT: run new fw boot_count=%lu\n", (unsigned long)f.boot_count);
        run_app(BSL_APP_BASE);   /* 继续尝试新固件 */
        return 1;
    }

    return 0;
}
