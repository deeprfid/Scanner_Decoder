/**
 * @file ota_agent.c
 * @brief F460 鍗囩骇 Agent锛氭敹鍖呭畬鎴� 鈫� 鏍￠獙 鈫� mark_ready 鈫� 澶嶄綅杩� boot commit
 *        锛團460 鏃犱富鍔ㄤ笅杞� ota_download锛汬TTP 閫氶亾鏀跺畬鍖呯洿鎺ヨ蛋杩欓噷锛�
 */
#include <string.h>
#include "hc32f46_driver.h"
#include "w25qxx.h"
#include "ota_layout.h"
#include "ota_storage.h"
#include "ota_state.h"
#include "ota_security.h"
#include "ota_agent.h"

#define OTA_HEADER_LEN      82
#define OTA_HDR_VERSION_OFF 4
#define OTA_HDR_PAYLOAD_OFF 10
#define OTA_HDR_CRC_OFF     14

/* v9.81p: 鏂板浐浠跺緟鑷纭鐘舵�� */
static int s_pending_confirm = 0;

/* 璇绘殏瀛樺寘澶� + 鏍￠獙 payload CRC + 楠岀锛涜緭鍑哄浐浠堕暱搴︿笌鐗堟湰 */
static int verify_staged_pkg(uint32_t *fw_len, uint32_t *version)
{
    uint8_t hdr[OTA_HEADER_LEN];
    uint32_t size, crc_expect;

    W25QXX_Read(hdr, OTA_QSPI_STAGE_BASE, sizeof(hdr));
    if (hdr[0] != 'O' || hdr[1] != 'T' || hdr[2] != 'A' || hdr[3] != '1')
        return -1;

    *version = ((uint32_t)hdr[OTA_HDR_VERSION_OFF]) | ((uint32_t)hdr[OTA_HDR_VERSION_OFF + 1] << 8) |
               ((uint32_t)hdr[OTA_HDR_VERSION_OFF + 2] << 16) | ((uint32_t)hdr[OTA_HDR_VERSION_OFF + 3] << 24);
    size = ((uint32_t)hdr[OTA_HDR_PAYLOAD_OFF]) | ((uint32_t)hdr[OTA_HDR_PAYLOAD_OFF + 1] << 8) |
           ((uint32_t)hdr[OTA_HDR_PAYLOAD_OFF + 2] << 16) | ((uint32_t)hdr[OTA_HDR_PAYLOAD_OFF + 3] << 24);
    *fw_len = size;

    crc_expect = ((uint32_t)hdr[OTA_HDR_CRC_OFF]) | ((uint32_t)hdr[OTA_HDR_CRC_OFF + 1] << 8) |
                 ((uint32_t)hdr[OTA_HDR_CRC_OFF + 2] << 16) | ((uint32_t)hdr[OTA_HDR_CRC_OFF + 3] << 24);
    if (ota_storage_verify_payload(OTA_HEADER_LEN, size, crc_expect) != 0) {
        TRACE("ota payload crc fail\n");
        return -1;
    }
    if (ota_security_verify_staged() != 0) {
        TRACE("ota verify security fail\n");
        return -1;
    }
    return 0;
}

/* F460: HTTP 閫氶亾鏀跺畬鍖呭悗璋冪敤锛堟浛浠� F4A0 鐨� ota_agent_run(url) 涓诲姩涓嬭浇锛� */
int ota_agent_finish(void)
{
    uint32_t fw_len = 0, version = 0;

    if (verify_staged_pkg(&fw_len, &version) != 0) {
        ota_fail(1);
        return -1;
    }
    if (ota_mark_ready(fw_len, version) != 0) {
        ota_fail(1);
        return -1;
    }
    TRACE("ota_agent ready ver:%lu size:%lu, reset...\n",
          (unsigned long)version, (unsigned long)fw_len);
    sleep_ms(500);
    system_reset();
    return 0;
}

void ota_agent_boot(void)
{
    /* bootloader handles NEED_COMMIT; app checks NEED_CONFIRM (self-test) */
    ota_state_t st = ota_state_run();
    if (st == OTA_STATE_READY)
        s_pending_confirm = 1;
}
void ota_agent_confirm(void)
{
    if (s_pending_confirm) {
        TRACE("ota_agent_confirm: self-check ok, confirm\n");
        ota_boot_confirm();
        ota_set_progress(0);
        s_pending_confirm = 0;
    }
}


