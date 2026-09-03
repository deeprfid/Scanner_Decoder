/**
 *******************************************************************************
 * @file  boot_qspi.h
 * @brief F460 Boot QSPI (W25Q64) 驱动：XIP 读 + 暂存区失效写
 *******************************************************************************
 */
#ifndef __BOOT_QSPI_H__
#define __BOOT_QSPI_H__

#include <stdint.h>

int  boot_qspi_init(void);                       /* 初始化 QSPI + 设置 W25Q64 QE(quad) */
int  boot_qspi_read(uint32_t u32Addr, uint8_t *pu8Buf, uint32_t u32Size);
void boot_qspi_invalidate_stage(void);           /* 清暂存区 magic（写 4 字节 0x00） */

#endif /* __BOOT_QSPI_H__ */
