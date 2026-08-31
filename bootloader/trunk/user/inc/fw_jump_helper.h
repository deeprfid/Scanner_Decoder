#ifndef _FW_JUMP_HELPER_
#define _FW_JUMP_HELPER_

void run_app(uint32_t appAddr);
int verifyFirmware(uint32 fwaddr, uint32 fwlen, uint32 fwcrc);
void fw_revert4bytes(uint8 *fwdata, int len);

#define FLASH_BASE            ((uint32_t)0x00000000) /*!< FLASH base address in the alias region */
#define SRAM_BASE             ((uint32_t)0x1FFF8000) /*!< SRAM base address in the alias region */
#define RAM_SIZE               0x2F000ul
#define PAGE_SIZE             (1024*8)

extern unsigned char Image$$RW_IRAM1$$ZI$$Limit;
extern uint8 armBootVersion[5];
extern uint8 gPageBuffer[PAGE_SIZE];

#endif

