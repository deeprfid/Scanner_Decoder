/**
 * @file ota_frame.h
 * @brief OTA1 本地帧协议核心（平台无关纯逻辑，F4A0/F460 复用）
 *
 * 帧：[0:4]"OTA1" [4]type [5:7]seq(16LE) [7:9]len(16LE) [9:9+N]payload [末2]CRC16-CCITT-FALSE(LE)
 * type: 0x50=DATA 0x51=ACK 0x52=RESUME；payload≤OTA_FRAME_MAX_PAYLOAD
 * 本模块不依赖任何芯片/驱动头文件。
 */
#ifndef OTA_FRAME_H
#define OTA_FRAME_H

#include <stdint.h>

#define OTA_FRAME_MAGIC0        'O'
#define OTA_FRAME_MAGIC1        'T'
#define OTA_FRAME_MAGIC2        'A'
#define OTA_FRAME_MAGIC3        '1'
#define OTA_FRAME_TYPE_DATA     0x50
#define OTA_FRAME_TYPE_ACK      0x51
#define OTA_FRAME_TYPE_RESUME   0x52
#define OTA_FRAME_HDR_LEN       9
#define OTA_FRAME_CRC_LEN       2
#define OTA_FRAME_MAX_PAYLOAD   4096   /* v9.81h: 2048->4096 串口提速（与 USB 帧对齐），static 缓冲已就绪 */
#define OTA_FRAME_MAX_LEN       (OTA_FRAME_HDR_LEN + OTA_FRAME_MAX_PAYLOAD + OTA_FRAME_CRC_LEN)

/* CRC16-CCITT-FALSE（poly 0x1021, init 0xFFFF） */
uint16_t ota_frame_crc16(const uint8_t *buf, uint32_t len);

/* 组帧：返回帧总长（≤OTA_FRAME_MAX_LEN）；失败 -1 */
int ota_frame_build(uint8_t type, uint16_t seq, const uint8_t *payload, uint16_t plen, uint8_t *out);

/* 解析帧：校验 magic/长度/CRC16；0=OK，<0 拒绝 */
int ota_frame_parse(const uint8_t *frame, uint16_t flen, uint8_t *type, uint16_t *seq, uint16_t *plen);

/* 流式组帧状态机（字节流通道：USB/网络） */
typedef struct {
    uint8_t  buf[OTA_FRAME_MAX_LEN];
    uint16_t len;
    uint16_t need;      /* 头部完成后还需字节（plen+CRC） */
    uint8_t  sync;      /* 魔数已同步 */
} ota_frame_feed_t;

void ota_frame_feed_init(ota_frame_feed_t *fx);
/* 喂 1 字节；返回 1=完整帧（frame_out/flen 输出），0=继续，<0 状态机内部复位（非法长度） */
int ota_frame_feed(ota_frame_feed_t *fx, uint8_t byte, uint8_t *frame_out, uint16_t *frame_len);

#endif /* OTA_FRAME_H */
