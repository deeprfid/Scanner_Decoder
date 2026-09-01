#ifndef _FW_JUMP_HELPER_
#define _FW_JUMP_HELPER_

void run_app(uint32_t appAddr);

#define SRAM_BASE             ((uint32_t)0x1FFF8000) /*!< SRAM base address in the alias region */
#define RAM_SIZE               0x2F000ul
#define PAGE_SIZE             (1024*8)

extern unsigned char Image$$RW_IRAM1$$ZI$$Limit;
extern uint8 gPageBuffer[PAGE_SIZE];

#endif

