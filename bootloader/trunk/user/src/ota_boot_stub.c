/**
 * @file ota_boot_stub.c
 * @brief boot 侧 stub：满足 boot 版 lib（IS_RTOS2=0）里 io_stream/irq 的 App 符号引用
 *        （这些符号属于 App 业务，boot 不需要实际功能，只提供空实现满足链接）
 */
#include "hc32f46_driver.h"

/* common.c #if IS_RTOS2_SUPPORT 内定义，io_stream.c extern 引用 */
uint8_t udp_tag_update = 0;

/* irq.c 引用 App 的 EAS 擦除（boot 无此功能，空实现） */
void Erase_eastag_to_flash(void)
{
}
