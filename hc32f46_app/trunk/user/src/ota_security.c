/**
 * @file ota_security.c
 * @brief OTA 安全层实现：HMAC-SHA256 验签（mbedtls md，流式读 QSPI 暂存）
 *        签名覆盖 [0:50]+payload（与统一 OTA 包 [50:82] 段比对）
 */
#include <string.h>
#include "hc32f46_driver.h"
#include "mbedtls/md.h"
#include "w25qxx.h"              /* F460: QSPI Flash 驱动 */
#include "ota_layout.h"          /* OTA_QSPI_STAGE_BASE/SIZE */
#include "ota_security.h"

/* 设备内置共享密钥（与 tools/ota_pack.py DEMO_HMAC_KEY 一致；正式化后按客户密钥替换） */
static const uint8_t OTA_SEC_KEY[] = "HC32F4A0_OTA_KEY_DEMO_20260814";
#define OTA_SEC_KEY_LEN   (sizeof(OTA_SEC_KEY) - 1)

static uint32_t rd_le32(const uint8_t *p)
{
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* F460 QSPI 读回调（暂存区，w25qxx） */
static int read_qspi(uint32_t off, uint8_t *buf, uint32_t len, void *arg)
{
    (void)arg;
    if ((off + len) > OTA_QSPI_STAGE_SIZE)
        return -1;
    /* w25qxx: 单次读 ≤0xFFFF（uint16），分段读 */
    while (len > 0) {
        uint32_t n = (len > 0xFFFFUL) ? 0xFFFFUL : len;
        W25QXX_Read(buf, OTA_QSPI_STAGE_BASE + off, (uint16_t)n);
        off += n; buf += n; len -= n;
    }
    return 0;
}

static int all_zero(const uint8_t *p, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        if (p[i]) return 0;
    return 1;
}

/* 验签：HMAC-SHA256(key, [0:50] + payload) vs [50:82] */
static int verify_signature(ota_sec_read_fn read, void *arg)
{
    uint8_t hdr[OTA_SEC_HEADER_LEN];
    uint8_t sig[OTA_SEC_SIGN_LEN];
    uint8_t calc[OTA_SEC_SIGN_LEN];
    uint8_t buf[512];
    uint32_t size, remain;
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *info;

    if (read(0, hdr, sizeof(hdr), arg) != 0)
        return -1;
    if (hdr[0] != 'O' || hdr[1] != 'T' || hdr[2] != 'A' || hdr[3] != '1')
        return -1;
    size = rd_le32(hdr + 10);
    if (size == 0 || size > OTA_QSPI_STAGE_SIZE - OTA_SEC_HEADER_LEN)
        return -1;
    if (read(OTA_SEC_SIGN_OFF, sig, OTA_SEC_SIGN_LEN, arg) != 0)
        return -1;

    if (all_zero(sig, OTA_SEC_SIGN_LEN)) {
#if (OTA_SECURITY_LEVEL >= 2)
        return -2;                     /* LEVEL2/3：强制签名，无签名拒绝 */
#else
        return 0;                      /* LEVEL1：兼容未签名包 */
#endif
    }

    info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (info == NULL)
        return -1;
    mbedtls_md_init(&ctx);
    if (mbedtls_md_setup(&ctx, info, 1) != 0)
        return -1;
    mbedtls_md_hmac_starts(&ctx, OTA_SEC_KEY, OTA_SEC_KEY_LEN);
    mbedtls_md_hmac_update(&ctx, hdr, OTA_SEC_SIGN_OFF);   /* [0:50] */
    remain = size;
    for (uint32_t off = OTA_SEC_HEADER_LEN; off < OTA_SEC_HEADER_LEN + size; off += sizeof(buf)) {
        uint32_t n = (remain > sizeof(buf)) ? sizeof(buf) : remain;
        if (read(off, buf, n, arg) != 0) {
            mbedtls_md_free(&ctx);
            return -1;
        }
        mbedtls_md_hmac_update(&ctx, buf, n);
        remain -= n;
    }
    mbedtls_md_hmac_finish(&ctx, calc);
    mbedtls_md_free(&ctx);

    return (memcmp(calc, sig, OTA_SEC_SIGN_LEN) == 0) ? 0 : -3;
}

/* SHA256 校验：payload SHA256 vs [18:50] */
static int verify_sha256(ota_sec_read_fn read, void *arg)
{
    uint8_t hdr[OTA_SEC_HEADER_LEN];
    uint8_t expect[OTA_SEC_SIGN_LEN];
    uint8_t calc[OTA_SEC_SIGN_LEN];
    uint8_t buf[512];
    uint32_t size, remain;
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *info;

    if (read(0, hdr, sizeof(hdr), arg) != 0)
        return -1;
    if (read(OTA_SEC_SHA256_OFF, expect, OTA_SEC_SIGN_LEN, arg) != 0)
        return -1;
    size = rd_le32(hdr + 10);

    info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (info == NULL)
        return -1;
    mbedtls_md_init(&ctx);
    if (mbedtls_md_setup(&ctx, info, 0) != 0)
        return -1;
    mbedtls_md_starts(&ctx);
    remain = size;
    for (uint32_t off = OTA_SEC_HEADER_LEN; off < OTA_SEC_HEADER_LEN + size; off += sizeof(buf)) {
        uint32_t n = (remain > sizeof(buf)) ? sizeof(buf) : remain;
        if (read(off, buf, n, arg) != 0) {
            mbedtls_md_free(&ctx);
            return -1;
        }
        mbedtls_md_update(&ctx, buf, n);
        remain -= n;
    }
    mbedtls_md_finish(&ctx, calc);
    mbedtls_md_free(&ctx);

    return (memcmp(calc, expect, OTA_SEC_SIGN_LEN) == 0) ? 0 : -4;
}

int ota_security_verify_ex(ota_sec_read_fn read, void *arg)
{
    int rc = verify_signature(read, arg);
    if (rc != 0)
        return rc;
    return verify_sha256(read, arg);
}

int ota_security_verify_staged(void)
{
    return ota_security_verify_ex(read_qspi, NULL);
}