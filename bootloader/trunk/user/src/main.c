#include <string.h>
#include "hc32_ddl.h"
#include "fw_jump_helper.h"
#include "w25qxx.h"
#include "ota_boot_single.h"
#include "app_conf.h"

/* boot_init.c */
void boot_hw_init(void);
void boot_printf(const char *fmt, ...);

/* Pure OTA boot: QSPI commit then run app (no cmd loop / wifi / ftp) */
int main(void)
{
    boot_hw_init();   /* clock + GPIO + SysTick + UART2 printf */

#if JustJump2App
    run_app(0x16000);
    while (1);
#endif

    W25QXX_Init();         /* QSPI flash init (after clock) */

    /* OTA single-bak: handle NEED_COMMIT/NEED_CONFIRM (1 = already ran app/reset) */
    if (ota_boot_single_run() != 0)
        return 0;

    /* No OTA task: run app directly */
    boot_printf("boot: no OTA, run app\n");
    run_app(0x16000);
    while (1);
}
