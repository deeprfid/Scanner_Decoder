/**
 *******************************************************************************
 * @file  boot_qspi.h
 * @brief F460 Boot QSPI (W25Q64) 驱动：XIP 读 + 暂存区失效写
 *******************************************************************************
 */
#ifndef __BOOT_QSPI_H__
#define __BOOT_QSPI_H__

#include <stdint.h>

/* QSPI 硬件自检开关（联调期已用，默认关闭；需要时可置 1） */
#ifndef BOOT_QSPI_DIAG
#define BOOT_QSPI_DIAG   0
#endif

int  boot_qspi_init(void);                       /* 初始化 QSPI + 设置 W25Q64 QE(quad) */
int  boot_qspi_read(uint32_t u32Addr, uint8_t *pu8Buf, uint32_t u32Size);   /* 标准读 0x03 */
int  boot_qspi_erase(uint32_t u32Addr, uint32_t u32Size);                   /* 4KB 扇区擦除 */
int  boot_qspi_write(uint32_t u32Addr, const uint8_t *pu8Src, uint32_t u32Size); /* 纯编程(不擦，调用方先擦) */
int  boot_qspi_write_region(uint32_t u32Addr, const uint8_t *pu8Src, uint32_t u32Size); /* 擦+写 */
void boot_qspi_diag(void);                       /* 硬件自检：JEDEC/QE/读/擦写回读 */

#endif /* __BOOT_QSPI_H__ */
