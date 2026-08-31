/**
 * @file ota_boot_single.h
 * @brief F460 bootloader single-bak OTA commit/rollback
 */
#ifndef OTA_BOOT_SINGLE_H
#define OTA_BOOT_SINGLE_H

/* 返回 1=已处理（run_app 或复位），0=无 OTA 需处理 */
int ota_boot_single_run(void);

#endif /* OTA_BOOT_SINGLE_H */
