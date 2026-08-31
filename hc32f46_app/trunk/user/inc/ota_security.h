/**
 * @file ota_security.h
 * @brief OTA 安全层（F4A0 实现）：包级 HMAC-SHA256 验签 + SHA256 完整性
 * @version V0.3  2026-08-15  签名正式化（与 doc/ota_interfaces/ota_security.h 对齐）
 *
 * 安全等级（编译期，app_conf.h 或本文件可覆盖）：
 *   OTA_SECURITY_LEVEL 1 = 网络+本地（默认）：有签名必须验，全 0 签名兼容放行
 *   OTA_SECURITY_LEVEL 2 = 仅本地：强制验签（无签名包拒绝）
 *   OTA_SECURITY_LEVEL 3 = 仅本地+强制签名（同 2）
 * 密钥：设备内置共享密钥（与 tools/ota_pack.py 的 DEMO_HMAC_KEY 一致）
 */
#ifndef OTA_SECURITY_H
#define OTA_SECURITY_H

#include <stdint.h>

#ifndef OTA_SECURITY_LEVEL
#define OTA_SECURITY_LEVEL 1
#endif

#define OTA_SEC_HEADER_LEN   82
#define OTA_SEC_SIGN_OFF     50
#define OTA_SEC_SIGN_LEN     32
#define OTA_SEC_SHA256_OFF   18

/* 读回调：off=暂存区相对偏移，0 为包头起始；返回 0=成功（平台无关，F460 可传 w25qxx 读） */
typedef int (*ota_sec_read_fn)(uint32_t off, uint8_t *buf, uint32_t len, void *arg);

/* 验签 + SHA256（任意读源，平台无关核心），0=通过；<0 拒绝（-2 无签名被拒） */
int ota_security_verify_ex(ota_sec_read_fn read, void *arg);

/* 便捷入口：读 QSPI 暂存区统一 OTA 包（F4A0 用） */
int ota_security_verify_staged(void);

#endif /* OTA_SECURITY_H */