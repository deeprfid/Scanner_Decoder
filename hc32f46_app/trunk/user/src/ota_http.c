/**
 * @file ota_http.c
 * @brief F460 HTTP OTA 閫氶亾锛氳澶� HTTP 鏈嶅姟鍣紝娴忚鍣�/涓婁綅鏈� POST 鏁村寘 .otapkg
 *        锛堜笌 F4A0 ota_http 鍚屾瀯锛涚敱 F460 httpServer 璺敱 /ota 璋冪敤 http_handle_conn锛�
 * Usage: 娴忚鍣� fetch POST http://<dev-ip>:<port>/ota  (body = .otapkg 鏂囦欢)
 */
#include <string.h>
#include "hc32f46_driver.h"
#include "w25qxx.h"
#include "ota_layout.h"
#include "ota_storage.h"
#include "ota_state.h"
#include "ota_http.h"
#include "ota_security.h"

#define OTA_HTTP_HDR_LEN     82
#define OTA_HTTP_ERASE_BLK   (65536UL)   /* 64KB block erase */

static uint32_t s_http_prog = 0, s_http_kv = 0;
static uint32_t s_http_erased = 0;   /* 鎳掓摝娓告爣 */

static long http_get_content_length(const char *hdr, int hdrlen)
{
    int i;
    for (i = 0; i < hdrlen - 15; i++) {
        if (strncmp(hdr + i, "Content-Length:", 15) == 0) {
            long v = 0;
            int j = i + 15;
            while (j < hdrlen && (hdr[j] == 32 || hdr[j] == 9)) j++;
            while (j < hdrlen && hdr[j] >= 48 && hdr[j] <= 57) { v = v * 10 + (hdr[j] - 48); j++; }
            return v;
        }
    }
    return -1;
}

/* 鎳掓摝闄わ細鎸� 4KB 鎵囧尯鎿︼紙鎽婂湪浼犺緭涓紝閬垮厤鏀跺畬涓�娆℃�ф摝鐨勫仠椤匡級 */
static int http_ensure_erased(uint32_t off, uint32_t len)
{
    (void)off; (void)len;
    if (s_http_erased != 0)
        return 0;
    /* 连接开始一次性擦完整个暂存区（8 x 64KB），收包中途不再擦，避免停顿导致上位机/W5100S 超时 */
    for (uint32_t a = 0; a < OTA_QSPI_STAGE_SIZE; a += 65536UL) {
        W25QXX_Erase_Block_64K(OTA_QSPI_STAGE_BASE + a);
    }
    W25QXX_Wait_Busy();
    s_http_erased = 1;
    return 0;
}

static int http_recv_body(int fd, long total)
{
    uint8_t buf[4096];
    long remain = total;

    if (ota_channel_try_acquire() != 0) {
        TRACE("ota_http: channel busy, reject\n");
        return -1;
    }
    /* body 鎺ユ敹鎭㈠闃诲妯″紡 + 瓒呮椂锛氶潪闃诲涓� read 杩斿洖 0 浼氳鍒や紶杈撳け璐� */
    {
        int blkmode = O_BLOCK;
        int to = 6000;   /* 30s recv body (SYSTEM_TICK_DUR=5ms) */
        ioctl(fd, COMMON_INTERFACE_SET_ISBLOCK, &blkmode);
        ioctl(fd, COMMON_INTERFACE_SET_TIMEOUT, &to);
    }
    ota_set_progress(0);
    s_http_prog = 0;
    s_http_kv   = 0;
    s_http_erased = 0;

    while (remain > 0) {
        int n = read(fd, buf, (uint32_t)((remain > (long)sizeof(buf)) ? sizeof(buf) : remain));
        if (n <= 0) {
            TRACE("ota_http: recv short %ld/%ld\n", (long)(total - remain), total);
            ota_channel_release();
            return -1;
        }
        if (http_ensure_erased(s_http_prog, (uint32_t)n) != 0) {
            ota_channel_release();
            return -1;
        }
        if (ota_storage_write_stage(buf, (uint32_t)n, s_http_prog) != 0) {
            TRACE("ota_http: stage write fail @%lu\n", (unsigned long)s_http_prog);
            ota_channel_release();
            return -1;
        }
        s_http_prog += (uint32_t)n;

        remain -= n;
        if (s_http_prog - s_http_kv >= 65536UL) {
            ota_set_progress(s_http_prog);
            s_http_kv = s_http_prog;
        }
    }
    if (s_http_prog != s_http_kv) {
        ota_set_progress(s_http_prog);
        s_http_kv = s_http_prog;
    }
    ota_channel_release();
    return 0;
}

/* one connection: header -> body to staging -> 200 -> verify -> mark_ready -> reset */
void http_handle_conn(int fd)
{
    char hdr[512];
    int  hlen = 0, n;
    long total;
    int  blkmode = O_NONBLOCK;   /* 鍏抽敭: socket 榛樿 O_BLOCK+timeout=-1 浼氭棤闄愮瓑鏁版嵁锛宧eader 蹇呴』闈為樆濉為槻鍗℃ */
    int  hto = 400;              /* 2s 鏀� header锛堝崟浣� SYSTEM_TICK_DUR=5ms锛� */

    ioctl(fd, COMMON_INTERFACE_SET_ISBLOCK, &blkmode);
    ioctl(fd, COMMON_INTERFACE_SET_TIMEOUT, &hto);

    while (hlen < (int)sizeof(hdr) - 1) {
        n = read(fd, (uint8_t *)hdr + hlen, 1);
        if (n != 1) break;
        hlen++;
        if (hlen >= 4 && hdr[hlen-4] == 13 && hdr[hlen-3] == 10 && hdr[hlen-2] == 13 && hdr[hlen-1] == 10)
            break;
    }
    hdr[hlen] = 0;
    if (hlen < 4) { TRACE("ota_http: no hdr\n"); return; }

    total = http_get_content_length(hdr, hlen);
    if (total <= OTA_HTTP_HDR_LEN) {
        TRACE("ota_http: bad len %ld\n", total);
        return;
    }
    TRACE("ota_http: POST %ld bytes\n", total);

    if (http_recv_body(fd, total) != 0)
        return;

    {
        static const char resp[] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
        (void)write(fd, resp, sizeof(resp) - 1);
    }

    {
        uint8_t h[OTA_HTTP_HDR_LEN];
        uint32_t ver, payload_len, crc_expect, fw_len;
        W25QXX_Read(h, OTA_QSPI_STAGE_BASE, sizeof(h));
        if (h[0] != 'O' || h[1] != 'T' || h[2] != 'A' || h[3] != '1') {
            TRACE("ota_http: no pkg hdr\n");
            return;
        }
        ver = (uint32_t)h[4] | ((uint32_t)h[5] << 8) | ((uint32_t)h[6] << 16) | ((uint32_t)h[7] << 24);
        payload_len = (uint32_t)h[10] | ((uint32_t)h[11] << 8) | ((uint32_t)h[12] << 16) | ((uint32_t)h[13] << 24);
        if (ver < OTA_FW_VERSION) {
            TRACE("ota_http: pkg ver 0x%08X != fw 0x%08X, reject\n",
                  (unsigned)ver, (unsigned)OTA_FW_VERSION);
            ota_set_progress(0);
            return;
        }
        crc_expect = (uint32_t)h[14] | ((uint32_t)h[15] << 8) | ((uint32_t)h[16] << 16) | ((uint32_t)h[17] << 24);
        fw_len = payload_len;
        TRACE("ota_http: verify %luB crc=%08X\n", (unsigned long)fw_len, (unsigned)crc_expect);
        if (ota_storage_verify_payload(OTA_HTTP_HDR_LEN, fw_len, crc_expect) != 0) {
            TRACE("ota_http: crc FAIL\n");
            ota_set_progress(0);
            return;
        }
        if (ota_security_verify_staged() != 0) {
            TRACE("ota_http: sec verify FAIL\n");
            ota_set_progress(0);
            return;
        }
        TRACE("ota_http: crc ok, mark_ready...\n");
        if (ota_mark_ready(fw_len, ver) != 0) { TRACE("ota_http: mark_ready FAIL\n"); return; }
        TRACE("ota_http: mark_ready ok, reset...\n");
        sleep_ms(500);
        system_reset();
    }
}
