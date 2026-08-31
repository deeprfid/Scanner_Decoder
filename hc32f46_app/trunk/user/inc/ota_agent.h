/**
 * @file ota_agent.h
 * @brief F460 OTA Agent：收包完成 → mark_ready → 复位；启动自检确认
 */
#ifndef OTA_AGENT_H
#define OTA_AGENT_H

#include <stdint.h>

/* HTTP 通道收完包后调用：校验 → mark_ready → system_reset */
int  ota_agent_finish(void);

/* 启动检查：暂存区有待确认固件则记 pending（业务就绪后 confirm） */
void ota_agent_boot(void);

/* 业务初始化完成后调用：新固件自检确认（防回滚） */
void ota_agent_confirm(void);

#endif /* OTA_AGENT_H */
