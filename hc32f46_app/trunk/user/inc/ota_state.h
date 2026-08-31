/**
 * @file ota_state.h
 * @brief MCU 升级状态机（EasyFlash IAP 思想 → FlashDB KV 落地，F4A0 首个实现）
 * @version V0.2  2026-08-14  Phase 3 起点
 */
#ifndef OTA_STATE_H
#define OTA_STATE_H

#include <stdint.h>
#include <stdbool.h>

/* FlashDB KV 键（与统一 OTA 协议一致，见 doc/ota_interfaces/ota_state.h） */
#define OTA_KV_NEED_COPY   "iap_need_copy"
#define OTA_KV_COPY_SIZE   "iap_copy_size"
#define OTA_KV_VERSION     "iap_version"
#define OTA_KV_PROGRESS    "iap_progress"
#define OTA_KV_SIGN_OK     "iap_sign_ok"
#define OTA_KV_RESULT      "iap_result"
#define OTA_KV_BOOT_COUNT  "iap_boot_count"

typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_READY,        /* 下载完+验签通过，待 commit */
    OTA_STATE_COMMITTING,
    OTA_STATE_SELF_CHECK,
} ota_state_t;

void ota_state_init(void);
ota_state_t ota_state_run(void);

/* 下载进度（断点续传，掉电保留） */
void ota_set_progress(uint32_t bytes);
uint32_t ota_get_progress(void);
uint32_t ota_get_copy_size(void);   /* iap_copy_size：待 commit 固件长度 */

/* 下载完成 + 验签通过 → 置 READY（写 KV + 请求复位进 bootloader） */
int  ota_mark_ready(uint32_t size, uint32_t version);

/* 自检通过/失败 */
void ota_self_check_ok(void);
/* 新固件自检确认（Phase 4）：清标志 NEED_CONFIRM + KV，防 bootloader 回滚 */
void ota_boot_confirm(void);
void ota_fail(uint8_t result);   /* 0=ok 1=fail 2=rollback */

/* boot count（bootloader 自检兜底） */
uint32_t ota_boot_count_get(void);
void ota_boot_count_inc(void);
void ota_boot_count_clear(void);

#endif /* OTA_STATE_H */


