/**
 *******************************************************************************
 * @file  boot_qspi.c
 * @brief F460 Boot QSPI (W25Q64 8MB) 精简驱动。
 *
 *   引脚（F460 JEUA QFN48，与 App w25qxx.c 一致）：
 *     QSCK=PB14  QSSN=PB01  QSIO0=PB13  QSIO1=PB12  QSIO2=PB10  QSIO3=PB02
 *     GPIO_FUNC_7，XIP 窗口 0x98000000。
 *   读：内存映射(XIP) QUAD IO Fast Read，24 位地址（W25Q64）。
 *   注：W25Q64JV 状态寄存器为易失（上电 QE=0），boot 每次启动自行置 QE，
 *       否则 QUAD 读无效。
 *******************************************************************************
 */
#include <string.h>
#include "hc32_ll.h"
#include "boot_ota.h"
#include "boot_qspi.h"

/* ---- F460 JEUA QFN48 QSPI 引脚 ---- */
#define BOOT_QSPI_SCK_PORT   (GPIO_PORT_B)
#define BOOT_QSPI_SCK_PIN    (GPIO_PIN_14)
#define BOOT_QSPI_CS_PORT    (GPIO_PORT_B)
#define BOOT_QSPI_CS_PIN     (GPIO_PIN_01)
#define BOOT_QSPI_IO0_PORT   (GPIO_PORT_B)
#define BOOT_QSPI_IO0_PIN    (GPIO_PIN_13)
#define BOOT_QSPI_IO1_PORT   (GPIO_PORT_B)
#define BOOT_QSPI_IO1_PIN    (GPIO_PIN_12)
#define BOOT_QSPI_IO2_PORT   (GPIO_PORT_B)
#define BOOT_QSPI_IO2_PIN    (GPIO_PIN_10)
#define BOOT_QSPI_IO3_PORT   (GPIO_PORT_B)
#define BOOT_QSPI_IO3_PIN    (GPIO_PIN_02)
#define BOOT_QSPI_FUNC       (GPIO_FUNC_7)

/* W25Q 指令 */
#define W25Q_WRITE_ENABLE       0x06U
#define W25Q_WRITE_STATUS2      0x31U
#define W25Q_READ_STATUS1       0x05U
#define W25Q_PAGE_PROGRAM       0x02U

/**
 * @brief 直接指令模式发一串字节
 */
static void qspi_cmd(const uint8_t *pu8Tx, uint32_t u32Len)
{
    uint32_t i;

    QSPI_EnterDirectCommMode();
    for (i = 0UL; i < u32Len; i++) {
        QSPI_WriteDirectCommValue(pu8Tx[i]);
    }
    QSPI_ExitDirectCommMode();
}

/**
 * @brief 轮询 SR1.BUSY 直到空闲
 */
static void qspi_wait_busy(void)
{
    uint32_t u32Cnt = 2000000UL;

    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(W25Q_READ_STATUS1);
    while (u32Cnt-- != 0UL) {
        if (0U == (QSPI_ReadDirectCommValue() & 0x01U)) {
            break;
        }
    }
    QSPI_ExitDirectCommMode();
}

/**
 * @brief 初始化 QSPI 外设 + W25Q64 置 QE（易失，每次上电需重设）
 */
int boot_qspi_init(void)
{
    stc_gpio_init_t stcGpioInit;
    stc_qspi_init_t stcQspiInit;
    uint8_t au8Cmd[2];

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDrv = PIN_HIGH_DRV;
    (void)GPIO_Init(BOOT_QSPI_SCK_PORT, BOOT_QSPI_SCK_PIN, &stcGpioInit);
    (void)GPIO_Init(BOOT_QSPI_CS_PORT,  BOOT_QSPI_CS_PIN,  &stcGpioInit);
    (void)GPIO_Init(BOOT_QSPI_IO0_PORT, BOOT_QSPI_IO0_PIN, &stcGpioInit);
    (void)GPIO_Init(BOOT_QSPI_IO1_PORT, BOOT_QSPI_IO1_PIN, &stcGpioInit);
    (void)GPIO_Init(BOOT_QSPI_IO2_PORT, BOOT_QSPI_IO2_PIN, &stcGpioInit);
    (void)GPIO_Init(BOOT_QSPI_IO3_PORT, BOOT_QSPI_IO3_PIN, &stcGpioInit);
    GPIO_SetFunc(BOOT_QSPI_SCK_PORT, BOOT_QSPI_SCK_PIN, BOOT_QSPI_FUNC);
    GPIO_SetFunc(BOOT_QSPI_CS_PORT,  BOOT_QSPI_CS_PIN,  BOOT_QSPI_FUNC);
    GPIO_SetFunc(BOOT_QSPI_IO0_PORT, BOOT_QSPI_IO0_PIN, BOOT_QSPI_FUNC);
    GPIO_SetFunc(BOOT_QSPI_IO1_PORT, BOOT_QSPI_IO1_PIN, BOOT_QSPI_FUNC);
    GPIO_SetFunc(BOOT_QSPI_IO2_PORT, BOOT_QSPI_IO2_PIN, BOOT_QSPI_FUNC);
    GPIO_SetFunc(BOOT_QSPI_IO3_PORT, BOOT_QSPI_IO3_PIN, BOOT_QSPI_FUNC);

    FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_QSPI, ENABLE);

    (void)QSPI_StructInit(&stcQspiInit);
    stcQspiInit.u32ClockDiv     = QSPI_CLK_DIV3;
    stcQspiInit.u32ReadMode     = QSPI_RD_MD_QUAD_IO_FAST_RD;
    stcQspiInit.u32PrefetchMode = QSPI_PREFETCH_MD_EDGE_STOP;
    stcQspiInit.u32DummyCycle   = QSPI_DUMMY_CYCLE6;
    stcQspiInit.u32AddrWidth    = QSPI_ADDR_WIDTH_24BIT;
    stcQspiInit.u32SetupTime    = QSPI_QSSN_SETUP_ADVANCE_QSCK1P5;
    stcQspiInit.u32ReleaseTime  = QSPI_QSSN_RELEASE_DELAY_QSCK1P5;
    stcQspiInit.u32IntervalTime = QSPI_QSSN_INTERVAL_QSCK2;
    (void)QSPI_Init(&stcQspiInit);

    /* W25Q64JV: WREN + 写 SR2(QE bit1) —— 每次上电需重设（状态寄存器易失） */
    au8Cmd[0] = W25Q_WRITE_ENABLE;
    qspi_cmd(au8Cmd, 1U);
    au8Cmd[0] = W25Q_WRITE_STATUS2;
    au8Cmd[1] = 0x02U;
    qspi_cmd(au8Cmd, 2U);
    qspi_wait_busy();
    return 0;
}

/**
 * @brief 内存映射(XIP)读暂存区
 */
int boot_qspi_read(uint32_t u32Addr, uint8_t *pu8Buf, uint32_t u32Size)
{
    if ((pu8Buf == NULL) || (u32Size == 0UL)) {
        return -1;
    }
    if ((u32Addr + u32Size) > (BOOT_QSPI_STAGE_BASE + BOOT_QSPI_STAGE_MAX)) {
        return -1;
    }
    memcpy(pu8Buf, (const void *)(QSPI_ROM_BASE + u32Addr), u32Size);
    return 0;
}

/**
 * @brief 失效暂存区：把 0x600000 处 4 字节 "OTA1" 编程为 0x00（NOR 可 1->0，无需擦除）
 */
void boot_qspi_invalidate_stage(void)
{
    uint8_t au8Cmd[8];

    /* WRITE_ENABLE */
    au8Cmd[0] = W25Q_WRITE_ENABLE;
    qspi_cmd(au8Cmd, 1U);
    /* PAGE_PROGRAM 0x02 + 24bit addr + 4 字节 0x00 */
    au8Cmd[0] = W25Q_PAGE_PROGRAM;
    au8Cmd[1] = (uint8_t)(BOOT_QSPI_STAGE_BASE >> 16U);
    au8Cmd[2] = (uint8_t)(BOOT_QSPI_STAGE_BASE >> 8U);
    au8Cmd[3] = (uint8_t)(BOOT_QSPI_STAGE_BASE);
    au8Cmd[4] = 0x00U;
    au8Cmd[5] = 0x00U;
    au8Cmd[6] = 0x00U;
    au8Cmd[7] = 0x00U;
    qspi_cmd(au8Cmd, 8U);
    qspi_wait_busy();
}

/******************************************************************************
 * EOF
 *****************************************************************************/
