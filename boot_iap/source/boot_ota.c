/**
 *******************************************************************************
 * @file  boot_ota.c
 * @brief F460 Bootloader OTA（single-bak）：状态区驱动 commit/回滚 + 跳 App。
 *
 *   契约与 App ota_layout.h / 归档 ota_boot_single.c 一致：
 *     App 下载完 -> 写暂存区(0x0) + 状态区 NEED_COMMIT -> 复位
 *     boot: NEED_COMMIT -> 校验暂存CRC -> 片内旧固件备份到 QSPI 0x80000
 *                          -> 暂存写片内 0x10000 -> 状态 NEED_CONFIRM -> 跳新固件
 *           NEED_CONFIRM -> boot_count++ 超限 -> 从备份恢复 -> 清状态 -> 跳 App
 *     App 自检成功 -> ota_agent_confirm 清状态位
 *
 *   烧写期间片内 Flash 忙：boot_ota.o / flash.o / hc32_ll_efm.o 由 scatter 放 RAM
 *   （HC32F460xE.sct RW_RAMCODE @0x20018000）。
 *******************************************************************************
 */
#include <stdio.h>
#include <string.h>
#include "hc32_ll.h"
#include "flash.h"
#include "boot_ota.h"
#include "boot_qspi.h"

extern void SWDT_FeedDog(void);

/* 跳转前外设解锁集合（与 main.c 的 LL_PERIPH_SEL 相同） */
#define BOOT_PERIPH_WE_SEL  (LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU | \
                             LL_PERIPH_EFM | LL_PERIPH_SRAM)

/* ---- 缓冲（4 对齐，EFM_Program 需要；1024B） ---- */
static uint32_t s_au32Buf[256];
#define COMMIT_BUF_SIZE   (1024UL)

/* ---- CRC32 查表（IEEE 0xEDB88320，与 ota_pack.py zlib.crc32 一致） ---- */
static const uint32_t s_au32CrcTab[256] = {
    0x0UL,0x77073096UL,0xEE0E612CUL,0x990951BAUL,0x76DC419UL,0x706AF48FUL,0xE963A535UL,0x9E6495A3UL,
    0xEDB8832UL,0x79DCB8A4UL,0xE0D5E91EUL,0x97D2D988UL,0x9B64C2BUL,0x7EB17CBDUL,0xE7B82D07UL,0x90BF1D91UL,
    0x1DB71064UL,0x6AB020F2UL,0xF3B97148UL,0x84BE41DEUL,0x1ADAD47DUL,0x6DDDE4EBUL,0xF4D4B551UL,0x83D385C7UL,
    0x136C9856UL,0x646BA8C0UL,0xFD62F97AUL,0x8A65C9ECUL,0x14015C4FUL,0x63066CD9UL,0xFA0F3D63UL,0x8D080DF5UL,
    0x3B6E20C8UL,0x4C69105EUL,0xD56041E4UL,0xA2677172UL,0x3C03E4D1UL,0x4B04D447UL,0xD20D85FDUL,0xA50AB56BUL,
    0x35B5A8FAUL,0x42B2986CUL,0xDBBBC9D6UL,0xACBCF940UL,0x32D86CE3UL,0x45DF5C75UL,0xDCD60DCFUL,0xABD13D59UL,
    0x26D930ACUL,0x51DE003AUL,0xC8D75180UL,0xBFD06116UL,0x21B4F4B5UL,0x56B3C423UL,0xCFBA9599UL,0xB8BDA50FUL,
    0x2802B89EUL,0x5F058808UL,0xC60CD9B2UL,0xB10BE924UL,0x2F6F7C87UL,0x58684C11UL,0xC1611DABUL,0xB6662D3DUL,
    0x76DC4190UL,0x1DB7106UL,0x98D220BCUL,0xEFD5102AUL,0x71B18589UL,0x6B6B51FUL,0x9FBFE4A5UL,0xE8B8D433UL,
    0x7807C9A2UL,0xF00F934UL,0x9609A88EUL,0xE10E9818UL,0x7F6A0DBBUL,0x86D3D2DUL,0x91646C97UL,0xE6635C01UL,
    0x6B6B51F4UL,0x1C6C6162UL,0x856530D8UL,0xF262004EUL,0x6C0695EDUL,0x1B01A57BUL,0x8208F4C1UL,0xF50FC457UL,
    0x65B0D9C6UL,0x12B7E950UL,0x8BBEB8EAUL,0xFCB9887CUL,0x62DD1DDFUL,0x15DA2D49UL,0x8CD37CF3UL,0xFBD44C65UL,
    0x4DB26158UL,0x3AB551CEUL,0xA3BC0074UL,0xD4BB30E2UL,0x4ADFA541UL,0x3DD895D7UL,0xA4D1C46DUL,0xD3D6F4FBUL,
    0x4369E96AUL,0x346ED9FCUL,0xAD678846UL,0xDA60B8D0UL,0x44042D73UL,0x33031DE5UL,0xAA0A4C5FUL,0xDD0D7CC9UL,
    0x5005713CUL,0x270241AAUL,0xBE0B1010UL,0xC90C2086UL,0x5768B525UL,0x206F85B3UL,0xB966D409UL,0xCE61E49FUL,
    0x5EDEF90EUL,0x29D9C998UL,0xB0D09822UL,0xC7D7A8B4UL,0x59B33D17UL,0x2EB40D81UL,0xB7BD5C3BUL,0xC0BA6CADUL,
    0xEDB88320UL,0x9ABFB3B6UL,0x3B6E20CUL,0x74B1D29AUL,0xEAD54739UL,0x9DD277AFUL,0x4DB2615UL,0x73DC1683UL,
    0xE3630B12UL,0x94643B84UL,0xD6D6A3EUL,0x7A6A5AA8UL,0xE40ECF0BUL,0x9309FF9DUL,0xA00AE27UL,0x7D079EB1UL,
    0xF00F9344UL,0x8708A3D2UL,0x1E01F268UL,0x6906C2FEUL,0xF762575DUL,0x806567CBUL,0x196C3671UL,0x6E6B06E7UL,
    0xFED41B76UL,0x89D32BE0UL,0x10DA7A5AUL,0x67DD4ACCUL,0xF9B9DF6FUL,0x8EBEEFF9UL,0x17B7BE43UL,0x60B08ED5UL,
    0xD6D6A3E8UL,0xA1D1937EUL,0x38D8C2C4UL,0x4FDFF252UL,0xD1BB67F1UL,0xA6BC5767UL,0x3FB506DDUL,0x48B2364BUL,
    0xD80D2BDAUL,0xAF0A1B4CUL,0x36034AF6UL,0x41047A60UL,0xDF60EFC3UL,0xA867DF55UL,0x316E8EEFUL,0x4669BE79UL,
    0xCB61B38CUL,0xBC66831AUL,0x256FD2A0UL,0x5268E236UL,0xCC0C7795UL,0xBB0B4703UL,0x220216B9UL,0x5505262FUL,
    0xC5BA3BBEUL,0xB2BD0B28UL,0x2BB45A92UL,0x5CB36A04UL,0xC2D7FFA7UL,0xB5D0CF31UL,0x2CD99E8BUL,0x5BDEAE1DUL,
    0x9B64C2B0UL,0xEC63F226UL,0x756AA39CUL,0x26D930AUL,0x9C0906A9UL,0xEB0E363FUL,0x72076785UL,0x5005713UL,
    0x95BF4A82UL,0xE2B87A14UL,0x7BB12BAEUL,0xCB61B38UL,0x92D28E9BUL,0xE5D5BE0DUL,0x7CDCEFB7UL,0xBDBDF21UL,
    0x86D3D2D4UL,0xF1D4E242UL,0x68DDB3F8UL,0x1FDA836EUL,0x81BE16CDUL,0xF6B9265BUL,0x6FB077E1UL,0x18B74777UL,
    0x88085AE6UL,0xFF0F6A70UL,0x66063BCAUL,0x11010B5CUL,0x8F659EFFUL,0xF862AE69UL,0x616BFFD3UL,0x166CCF45UL,
    0xA00AE278UL,0xD70DD2EEUL,0x4E048354UL,0x3903B3C2UL,0xA7672661UL,0xD06016F7UL,0x4969474DUL,0x3E6E77DBUL,
    0xAED16A4AUL,0xD9D65ADCUL,0x40DF0B66UL,0x37D83BF0UL,0xA9BCAE53UL,0xDEBB9EC5UL,0x47B2CF7FUL,0x30B5FFE9UL,
    0xBDBDF21CUL,0xCABAC28AUL,0x53B39330UL,0x24B4A3A6UL,0xBAD03605UL,0xCDD70693UL,0x54DE5729UL,0x23D967BFUL,
    0xB3667A2EUL,0xC4614AB8UL,0x5D681B02UL,0x2A6F2B94UL,0xB40BBE37UL,0xC30C8EA1UL,0x5A05DF1BUL,0x2D02EF8DUL
};

static uint32_t boot_crc32_update(uint32_t u32Crc, const uint8_t *pu8Data, uint32_t u32Len)
{
    uint32_t i;

    for (i = 0UL; i < u32Len; i++) {
        u32Crc = s_au32CrcTab[(u32Crc ^ pu8Data[i]) & 0xFFUL] ^ (u32Crc >> 8UL);
    }
    return u32Crc;
}

static uint32_t boot_rd_le32(const uint8_t *pu8)
{
    return ((uint32_t)pu8[0]) | ((uint32_t)pu8[1] << 8U) | ((uint32_t)pu8[2] << 16U) |
           ((uint32_t)pu8[3] << 24U);
}

/* ---- 状态区 ---- */
static void state_write_raw(const boot_state_t *pstcState)
{
    (void)boot_qspi_erase(BOOT_QSPI_STATE_BASE, BOOT_QSPI_STATE_SIZE);
    (void)boot_qspi_write(BOOT_QSPI_STATE_BASE, (const uint8_t *)pstcState,
                          sizeof(boot_state_t));
    SWDT_FeedDog();
}

/* 写状态（自动补 crc32 over [0,20)） */
static void state_write(const boot_state_t *pstcState)
{
    boot_state_t stcTmp = *pstcState;

    stcTmp.crc32 = boot_crc32_update(0xFFFFFFFFUL, (const uint8_t *)pstcState,
                                      BOOT_STATE_CRC_LEN) ^ 0xFFFFFFFFUL;
    state_write_raw(&stcTmp);
}

/* 读状态；无效(magic/crc)返回非 0 */
static int state_read(boot_state_t *pstcState)
{
    if (pstcState == NULL) {
        return -1;
    }
    if (boot_qspi_read(BOOT_QSPI_STATE_BASE, (uint8_t *)pstcState, sizeof(boot_state_t)) != 0) {
        return -1;
    }
    if (pstcState->magic != BOOT_FLAG_MAGIC) {
        return -1;
    }
    if (pstcState->crc32 !=
        (boot_crc32_update(0xFFFFFFFFUL, (const uint8_t *)pstcState,
                           BOOT_STATE_CRC_LEN) ^ 0xFFFFFFFFUL)) {
        return -1;
    }
    return 0;
}

/* 清状态（写 magic-only，flags=0 —— 与归档语义一致：无动作位即正常跑） */
static void state_clear(void)
{
    boot_state_t stcTmp;

    memset(&stcTmp, 0, sizeof(stcTmp));
    stcTmp.magic = BOOT_FLAG_MAGIC;
    state_write(&stcTmp);
}

/* ---- 暂存包校验：magic + 长度 + payload CRC32 ---- */
static int verify_staged(uint32_t *pu32FwLen, uint32_t *pu32Version)
{
    uint8_t au8Hdr[BOOT_OTA_HDR_LEN];
    uint32_t u32Len;
    uint32_t u32CrcExp;
    uint32_t u32Crc;
    uint32_t u32Off;
    uint32_t u32N;
    uint32_t u32Remain;

    if (boot_qspi_read(BOOT_QSPI_STAGE_BASE, au8Hdr, BOOT_OTA_HDR_LEN) != 0) {
        return -1;
    }
    if ((au8Hdr[0] != BOOT_OTA_MAGIC0) || (au8Hdr[1] != BOOT_OTA_MAGIC1) ||
        (au8Hdr[2] != BOOT_OTA_MAGIC2) || (au8Hdr[3] != BOOT_OTA_MAGIC3)) {
        return -1;
    }
    *pu32Version = boot_rd_le32(&au8Hdr[BOOT_OTA_VER_OFF]);
    u32Len       = boot_rd_le32(&au8Hdr[BOOT_OTA_LEN_OFF]);
    u32CrcExp    = boot_rd_le32(&au8Hdr[BOOT_OTA_CRC_OFF]);
    if ((u32Len == 0UL) || (u32Len > BOOT_APP_MAX_SIZE) ||
        ((BOOT_OTA_HDR_LEN + u32Len) > BOOT_QSPI_STAGE_SIZE)) {
        return -1;
    }
    *pu32FwLen = u32Len;

    u32Crc    = 0xFFFFFFFFUL;
    u32Remain = u32Len;
    for (u32Off = BOOT_OTA_HDR_LEN; u32Remain > 0UL; u32Off += u32N) {
        u32N = (u32Remain > COMMIT_BUF_SIZE) ? COMMIT_BUF_SIZE : u32Remain;
        if (boot_qspi_read(BOOT_QSPI_STAGE_BASE + u32Off, (uint8_t *)s_au32Buf, u32N) != 0) {
            return -1;
        }
        u32Crc = boot_crc32_update(u32Crc, (uint8_t *)s_au32Buf, u32N);
        u32Remain -= u32N;
    }
    u32Crc ^= 0xFFFFFFFFUL;
    return (u32Crc == u32CrcExp) ? 0 : -2;
}

/* ---- 片内 App 区写入（RAM 执行）：u32Addr 8KB 扇区对齐由调用方保证 ---- */
static int app_region_write(uint32_t u32Addr, uint32_t u32Len,
                            int32_t (*pfnRead)(uint32_t, uint8_t *, uint32_t))
{
    uint32_t u32Off;
    uint32_t u32N;
    uint32_t u32Remain;
    uint32_t u32EraseSize;
    int32_t i32Ret;

    /* 先整区擦除（8KB 扇区，向上取整） */
    u32EraseSize = (u32Len + FLASH_SECTOR_SIZE - 1UL) & ~(FLASH_SECTOR_SIZE - 1UL);
    SWDT_FeedDog();
    if (LL_OK != FLASH_EraseSector(u32Addr, u32EraseSize)) {
        return -1;
    }
    SWDT_FeedDog();

    u32Remain = u32Len;
    u32Off    = 0UL;
    while (u32Remain > 0UL) {
        u32N = (u32Remain > COMMIT_BUF_SIZE) ? COMMIT_BUF_SIZE : u32Remain;
        if (pfnRead(u32Off, (uint8_t *)s_au32Buf, u32N) != 0) {
            return -1;
        }
        i32Ret = FLASH_WriteData(u32Addr + u32Off, (uint8_t *)s_au32Buf, u32N);
        if (LL_OK != i32Ret) {
            printf("BOOT: program fail off=%u ret=%d\r\n", (unsigned int)u32Off, (int)i32Ret);
            return -1;
        }
        SWDT_FeedDog();
        u32Off    += u32N;
        u32Remain -= u32N;
    }
    return 0;
}

/* 从暂存区读 payload（跳 82B 头） */
static int32_t read_from_stage(uint32_t u32Off, uint8_t *pu8Buf, uint32_t u32Len)
{
    return boot_qspi_read(BOOT_QSPI_STAGE_BASE + BOOT_OTA_HDR_LEN + u32Off, pu8Buf, u32Len);
}

/* 从片内 App 区读（备份用） */
static int32_t read_from_app(uint32_t u32Off, uint8_t *pu8Buf, uint32_t u32Len)
{
    memcpy(pu8Buf, (const void *)(BOOT_APP_BASE + u32Off), u32Len);
    return 0;
}

/* 从备份区读（回滚用） */
static int32_t read_from_backup(uint32_t u32Off, uint8_t *pu8Buf, uint32_t u32Len)
{
    return boot_qspi_read(BOOT_QSPI_BACKUP_BASE + u32Off, pu8Buf, u32Len);
}

/* ---- commit：备份旧固件 -> 暂存写片内 -> NEED_CONFIRM ---- */
static int commit_new_fw(uint32_t u32FwLen, uint32_t u32Version)
{
    uint32_t u32Off;
    uint32_t u32N;
    uint32_t u32Remain;
    boot_state_t stcState;
    int32_t i32Ret;

    printf("BOOT: NEED_COMMIT ver=0x%08X len=%u\r\n", (unsigned int)u32Version,
           (unsigned int)u32FwLen);

    /* 1) 片内当前 App (fw_len 字节) -> QSPI 备份区（先整区擦除） */
    printf("BOOT: backup current app -> QSPI 0x%06X\r\n", (unsigned int)BOOT_QSPI_BACKUP_BASE);
    if (boot_qspi_erase(BOOT_QSPI_BACKUP_BASE, BOOT_QSPI_BACKUP_SIZE) != 0) {
        return -1;
    }
    u32Remain = u32FwLen;
    u32Off    = 0UL;
    while (u32Remain > 0UL) {
        u32N = (u32Remain > COMMIT_BUF_SIZE) ? COMMIT_BUF_SIZE : u32Remain;
        if (read_from_app(u32Off, (uint8_t *)s_au32Buf, u32N) != 0) {
            return -1;
        }
        if (boot_qspi_write(BOOT_QSPI_BACKUP_BASE + u32Off, (const uint8_t *)s_au32Buf,
                            u32N) != 0) {
            return -1;
        }
        SWDT_FeedDog();
        u32Off    += u32N;
        u32Remain -= u32N;
    }

    /* 2) 暂存(跳 82B 头) -> 片内 0x10000（EFM 需先解锁） */
    LL_PERIPH_WE(BOOT_PERIPH_WE_SEL);
    EFM_FWMC_Cmd(ENABLE);
    printf("BOOT: write new fw -> internal flash\r\n");
    i32Ret = app_region_write(BOOT_APP_BASE, u32FwLen, read_from_stage);
    EFM_FWMC_Cmd(DISABLE);
    LL_PERIPH_WP(BOOT_PERIPH_WE_SEL);
    if (i32Ret != 0) {
        return -1;
    }

    /* 3) 状态 NEED_CONFIRM（新固件自检） */
    memset(&stcState, 0, sizeof(stcState));
    stcState.magic      = BOOT_FLAG_MAGIC;
    stcState.flags      = BOOT_FLAG_NEED_CONFIRM;
    stcState.size       = u32FwLen;
    stcState.version    = u32Version;
    stcState.boot_count = 0UL;
    state_write(&stcState);
    printf("BOOT: commit ok -> NEED_CONFIRM\r\n");
    return 0;
}

/* ---- 回滚：备份区 -> 片内，清状态 ---- */
static void rollback(uint32_t u32FwLen)
{
    printf("BOOT: rollback from backup\r\n");
    LL_PERIPH_WE(BOOT_PERIPH_WE_SEL);
    EFM_FWMC_Cmd(ENABLE);
    (void)app_region_write(BOOT_APP_BASE, u32FwLen, read_from_backup);
    EFM_FWMC_Cmd(DISABLE);
    LL_PERIPH_WP(BOOT_PERIPH_WE_SEL);
    state_clear();
    printf("BOOT: rollback done\r\n");
}

/**
 * @brief 跳转 App（照官方 iap_ymodem_boot：先降频回 MRC、关 PLL/XTAL，再设 MSP+VTOR）
 * @note  不返回；App 无效则死循环（等外部复位/烧录）
 */
static void boot_ota_jump_app(void)
{
    uint32_t u32Stack = *((volatile uint32_t *)BOOT_APP_BASE);
    uint32_t u32Reset = *((volatile uint32_t *)(BOOT_APP_BASE + 4UL));
    void (*pfnApp)(void);

    if ((u32Stack > BOOT_SRAM_BASE) && (u32Stack <= BOOT_SRAM_TOP)) {
        if ((u32Reset >= BOOT_APP_BASE) && (u32Reset < 0x00080000UL) &&
            (0UL != (u32Reset & 1UL))) {
            printf("BOOT: jump app\r\n");
						DDL_DelayMS(100);	
            /* 降频回 MRC + 关 PLL/XTAL（官方 IAP_CLK_DeInit 流程） */
            LL_PERIPH_WE(BOOT_PERIPH_WE_SEL);
            CLK_SetSysClockSrc(CLK_SYSCLK_SRC_MRC);
            (void)PWC_HighPerformanceToHighSpeed();
            CLK_SetClockDiv(CLK_BUS_CLK_ALL, (CLK_HCLK_DIV1 | CLK_EXCLK_DIV1 | CLK_PCLK0_DIV1 |
                                             CLK_PCLK1_DIV1 | CLK_PCLK2_DIV1 | CLK_PCLK3_DIV1 |
                                             CLK_PCLK4_DIV1));
            CLK_PLLCmd(DISABLE);
            CLK_XtalCmd(DISABLE);
            SRAM_SetWaitCycle(SRAM_SRAM_ALL, SRAM_WAIT_CYCLE0, SRAM_WAIT_CYCLE0);
            SRAM_SetWaitCycle(SRAM_SRAMH, SRAM_WAIT_CYCLE0, SRAM_WAIT_CYCLE0);
            GPIO_SetReadWaitCycle(GPIO_RD_WAIT0);
            (void)EFM_SetWaitCycle(EFM_WAIT_CYCLE0);
            EFM_FWMC_Cmd(DISABLE);
            LL_PERIPH_WP(BOOT_PERIPH_WE_SEL);

            __disable_irq();
            __set_MSP(u32Stack);
            /* App 中断向量指向自身（App 无 VTOR 处理，boot 负责） */
            SCB->VTOR = BOOT_APP_BASE;
            __DSB();
            pfnApp = (void (*)(void))u32Reset;
            pfnApp();
        }
    }
    printf("BOOT: no valid app @0x%08X - halt\r\n", (unsigned int)BOOT_APP_BASE);
    for (;;) {
    }
}

/**
 * @brief 引导入口（不返回）
 */
void BOOT_OTA_Run(void)
{
    boot_state_t stcState;
    uint32_t u32FwLen  = 0UL;
    uint32_t u32Ver    = 0UL;
    int32_t i32Verify;

    printf("=========== BOOT OTA ===========\r\n");

#if (BOOT_QSPI_DIAG == 1)
    boot_qspi_diag();
#endif

    if (state_read(&stcState) != 0) {
        /* 无有效状态 -> 正常跳 App */
        boot_ota_jump_app();
        return;
    }

    if (0UL != (stcState.flags & BOOT_FLAG_NEED_COMMIT)) {
        i32Verify = verify_staged(&u32FwLen, &u32Ver);
        if (i32Verify != 0) {
            printf("BOOT: staged verify FAIL(%d), clear flag, keep old app\r\n", (int)i32Verify);
            state_clear();
            boot_ota_jump_app();
            return;
        }
        if (commit_new_fw(u32FwLen, u32Ver) != 0) {
            printf("BOOT: commit FAIL (keep NEED_COMMIT, retry next boot)\r\n");
            boot_ota_jump_app();   /* 旧 App 可能已部分损坏；下次上电重试 commit */
            return;
        }
        boot_ota_jump_app();   /* 跳新固件自检（NEED_CONFIRM 在 commit 内已置） */
        return;
    }

    if (0UL != (stcState.flags & BOOT_FLAG_NEED_CONFIRM)) {
        stcState.boot_count++;
        if (stcState.boot_count > BOOT_MAX_BOOT_COUNT) {
            printf("BOOT: self-test fail, boot_count=%u exceeded -> rollback\r\n",
                   (unsigned int)stcState.boot_count);
            rollback(stcState.size);
            boot_ota_jump_app();
            return;
        }
        state_write(&stcState);
        printf("BOOT: run new fw self-test boot_count=%u\r\n",
               (unsigned int)stcState.boot_count);
        boot_ota_jump_app();
        return;
    }

    /* 状态有效但无动作位（App 已确认/清位） */
    boot_ota_jump_app();
}

/******************************************************************************
 * EOF
 *****************************************************************************/
