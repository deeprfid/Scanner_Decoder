#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "hc32f46_driver.h"
#include "fw_jump_helper.h"
#include "w25qxx.h"          /* OTA: QSPI flash */
#include "ota_boot_single.h" /* OTA: single-bak commit */
#include "app_conf.h"
#include <rt_heap.h>
extern unsigned char Image$$ER_IROM1$$Base;


/* Pure OTA boot: QSPI commit then run app (no cmd loop / wifi / ftp) */
int main(void)
{
	
	
    RCC_Configuration();   /* system clock */
    GPIO_Configuration();
    int heap_base_address = ((int)&Image$$RW_IRAM1$$ZI$$Limit);
    heap_base_address += 64 - heap_base_address % 64;
    _init_alloc(heap_base_address, 0x20026FF0);


#if JustJump2App
    run_app(0x16000);
    while (1);
#endif

    timer_Init();
    W25QXX_Init();         /* QSPI flash init (after clock) */

    /* OTA single-bak: handle NEED_COMMIT/NEED_CONFIRM (1 = already ran app/reset) */
    if (ota_boot_single_run() != 0)
        return 0;

    /* No OTA task: run app directly */
    TRACE("boot: no OTA, run app\n");
    run_app(0x16000);
    while (1);
}
