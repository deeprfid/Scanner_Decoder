/**
 * @file ota_frame.c
 * @brief OTA1 帧协议核心实现（纯逻辑，从 ota_transport_uart.c 抽出，F460 复用）
 */
#include <string.h>
#include "ota_frame.h"

/* v9.82: 查表 CRC-16/CCITT-FALSE（与逐位版结果一致，快 ~8 倍） */
static uint16_t s_crc16_tab[256];
static int s_crc16_tab_init = 0;

static void crc16_tab_init_once(void)
{
    if (s_crc16_tab_init) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint16_t c = (uint16_t)(i << 8);
        for (int k = 0; k < 8; k++)
            c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
        s_crc16_tab[i] = c;
    }
    s_crc16_tab_init = 1;
}

uint16_t ota_frame_crc16(const uint8_t *buf, uint32_t len)
{
    uint16_t crc = 0xFFFF;
    crc16_tab_init_once();
    for (uint32_t i = 0; i < len; i++)
        crc = (uint16_t)((crc << 8) ^ s_crc16_tab[((crc >> 8) ^ buf[i]) & 0xFF]);
    return crc;
}

int ota_frame_build(uint8_t type, uint16_t seq, const uint8_t *payload, uint16_t plen, uint8_t *out)
{
    uint16_t crc;
    if (plen > OTA_FRAME_MAX_PAYLOAD || out == NULL)
        return -1;
    out[0] = OTA_FRAME_MAGIC0;
    out[1] = OTA_FRAME_MAGIC1;
    out[2] = OTA_FRAME_MAGIC2;
    out[3] = OTA_FRAME_MAGIC3;
    out[4] = type;
    out[5] = (uint8_t)(seq & 0xFF);
    out[6] = (uint8_t)((seq >> 8) & 0xFF);
    out[7] = (uint8_t)(plen & 0xFF);
    out[8] = (uint8_t)((plen >> 8) & 0xFF);
    if (plen)
        memcpy(out + OTA_FRAME_HDR_LEN, payload, plen);
    crc = ota_frame_crc16(out, OTA_FRAME_HDR_LEN + plen);
    out[OTA_FRAME_HDR_LEN + plen]     = (uint8_t)(crc & 0xFF);
    out[OTA_FRAME_HDR_LEN + plen + 1] = (uint8_t)((crc >> 8) & 0xFF);
    return OTA_FRAME_HDR_LEN + plen + OTA_FRAME_CRC_LEN;
}

int ota_frame_parse(const uint8_t *frame, uint16_t flen, uint8_t *type, uint16_t *seq, uint16_t *plen)
{
    uint16_t p, crc_rx, crc_calc;
    if (flen < OTA_FRAME_HDR_LEN + OTA_FRAME_CRC_LEN)
        return -1;
    if (frame[0] != OTA_FRAME_MAGIC0 || frame[1] != OTA_FRAME_MAGIC1 ||
        frame[2] != OTA_FRAME_MAGIC2 || frame[3] != OTA_FRAME_MAGIC3)
        return -1;
    p = (uint16_t)(frame[7] | ((uint16_t)frame[8] << 8));
    if (flen != OTA_FRAME_HDR_LEN + p + OTA_FRAME_CRC_LEN)
        return -1;
    crc_rx   = (uint16_t)(frame[OTA_FRAME_HDR_LEN + p] | ((uint16_t)frame[OTA_FRAME_HDR_LEN + p + 1] << 8));
    crc_calc = ota_frame_crc16(frame, OTA_FRAME_HDR_LEN + p);
    if (crc_rx != crc_calc)
        return -2;
    if (type) *type = frame[4];
    if (seq)  *seq  = (uint16_t)(frame[5] | ((uint16_t)frame[6] << 8));
    if (plen) *plen = p;
    return 0;
}

void ota_frame_feed_init(ota_frame_feed_t *fx)
{
    if (fx == NULL) return;
    memset(fx, 0, sizeof(*fx));
}

int ota_frame_feed(ota_frame_feed_t *fx, uint8_t byte, uint8_t *frame_out, uint16_t *frame_len)
{
    if (fx == NULL || frame_out == NULL || frame_len == NULL)
        return -1;

    if (!fx->sync) {
        if (fx->len == 0 && byte == OTA_FRAME_MAGIC0)      { fx->buf[0] = byte; fx->len = 1; }
        else if (fx->len == 1 && byte == OTA_FRAME_MAGIC1) { fx->buf[1] = byte; fx->len = 2; }
        else if (fx->len == 2 && byte == OTA_FRAME_MAGIC2) { fx->buf[2] = byte; fx->len = 3; }
        else if (fx->len == 3 && byte == OTA_FRAME_MAGIC3) { fx->buf[3] = byte; fx->len = 4; fx->sync = 1; }
        else                                                fx->len = 0;
        return 0;
    }

    if (fx->len >= OTA_FRAME_MAX_LEN) {
        ota_frame_feed_init(fx);
        return -2;                       /* 超长复位 */
    }
    fx->buf[fx->len++] = byte;

    if (fx->len == OTA_FRAME_HDR_LEN) {
        uint16_t plen = (uint16_t)(fx->buf[7] | ((uint16_t)fx->buf[8] << 8));
        if (plen > OTA_FRAME_MAX_PAYLOAD) {
            ota_frame_feed_init(fx);
            return -2;                   /* 非法长度 */
        }
        fx->need = (uint16_t)(plen + OTA_FRAME_CRC_LEN);
    } else if (fx->need > 0 && fx->len == (uint16_t)(OTA_FRAME_HDR_LEN + fx->need)) {
        memcpy(frame_out, fx->buf, fx->len);
        *frame_len = fx->len;
        ota_frame_feed_init(fx);
        return 1;                        /* 完整帧 */
    }
    return 0;
}