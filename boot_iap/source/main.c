/**
 *******************************************************************************
 * @file  iap_boot/source/main.c
 * @brief Boot alive
 *
 * 时钟：MPLL@200MHz（完全照 BSP_CLK_Init / usart_uart_polling 例程）
 * printf：DDL_PrintfInit 官方方式（USART3 @115200）
 * 喂狗：串口就绪后立即 + 主循环每轮（对照 App/driver）
 *******************************************************************************
 */

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "hc32_ll.h"
#include <stdio.h>

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
#define LL_PERIPH_SEL                   (LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU | \
                                         LL_PERIPH_EFM | LL_PERIPH_SRAM)

/* 本板 debug printf 用 USART3: TX=PB7(GPIO_FUNC_32) */
#define PRINTF_USART                    (CM_USART3)
#define PRINTF_USART_FCG                (FCG1_PERIPH_USART3)
#define PRINTF_TX_PORT                  (GPIO_PORT_B)
#define PRINTF_TX_PIN                   (GPIO_PIN_07)
#define PRINTF_TX_FUNC                  (GPIO_FUNC_32)
#define PRINTF_BAUDRATE                 (115200UL)
#define PRINTF_BAUDRATE_ERR_MAX         (0.025F)

/* XTAL pins (PH0/PH1) */
#define BSP_XTAL_PORT                   (GPIO_PORT_H)
#define BSP_XTAL_PIN                    (GPIO_PIN_00 | GPIO_PIN_01)

/*******************************************************************************
 * Function implementation
 ******************************************************************************/

/**
 * @brief  系统时钟：MPLL@200MHz（与 BSP_CLK_Init / usart_uart_polling 例程完全一致）
 */
static void SystemClockConfig(void)
{
    stc_clock_xtal_init_t stcXtalInit;
    stc_clock_pll_init_t  stcMpllInit;

    GPIO_AnalogCmd(BSP_XTAL_PORT, BSP_XTAL_PIN, ENABLE);
    (void)CLK_XtalStructInit(&stcXtalInit);
    (void)CLK_PLLStructInit(&stcMpllInit);

    /* Set bus clk div. */
    CLK_SetClockDiv(CLK_BUS_CLK_ALL, (CLK_HCLK_DIV1 | CLK_EXCLK_DIV2 | CLK_PCLK0_DIV1 | CLK_PCLK1_DIV2 | \
                                      CLK_PCLK2_DIV4 | CLK_PCLK3_DIV4 | CLK_PCLK4_DIV2));

    /* Config Xtal and enable Xtal */
    stcXtalInit.u8Mode = CLK_XTAL_MD_OSC;
    stcXtalInit.u8Drv = CLK_XTAL_DRV_ULOW;
    stcXtalInit.u8State = CLK_XTAL_ON;
    stcXtalInit.u8StableTime = CLK_XTAL_STB_2MS;
    (void)CLK_XtalInit(&stcXtalInit);

    /* MPLL config (XTAL / pllmDiv * plln / PllpDiv = 200M): 8M/1*50/2=200M */
    stcMpllInit.PLLCFGR = 0UL;
    stcMpllInit.PLLCFGR_f.PLLM = 1UL - 1UL;
    stcMpllInit.PLLCFGR_f.PLLN = 50UL - 1UL;
    stcMpllInit.PLLCFGR_f.PLLP = 2UL - 1UL;
    stcMpllInit.PLLCFGR_f.PLLQ = 2UL - 1UL;
    stcMpllInit.PLLCFGR_f.PLLR = 2UL - 1UL;
    stcMpllInit.u8PLLState = CLK_PLL_ON;
    stcMpllInit.PLLCFGR_f.PLLSRC = CLK_PLL_SRC_XTAL;
    (void)CLK_PLLInit(&stcMpllInit);
    /* Wait MPLL ready. */
		 SWDT_FeedDog();
    while (SET != CLK_GetStableStatus(CLK_STB_FLAG_PLL)) {
        ;
    }
     SWDT_FeedDog();
    /* sram init include read/write wait cycle setting */
    SRAM_SetWaitCycle(SRAM_SRAMH, SRAM_WAIT_CYCLE0, SRAM_WAIT_CYCLE0);
    SRAM_SetWaitCycle((SRAM_SRAM12 | SRAM_SRAM3 | SRAM_SRAMR), SRAM_WAIT_CYCLE1, SRAM_WAIT_CYCLE1);

    /* flash read wait cycle setting */
    (void)EFM_SetWaitCycle(EFM_WAIT_CYCLE5);
    /* 3 cycles for 126MHz ~ 200MHz */
    GPIO_SetReadWaitCycle(GPIO_RD_WAIT3);
    /* Switch driver ability */
    (void)PWC_HighSpeedToHighPerformance();
    /* Switch system clock source to MPLL. */
    CLK_SetSysClockSrc(CLK_SYSCLK_SRC_PLL);
    /* Reset cache ram */
    EFM_CacheRamReset(ENABLE);
    EFM_CacheRamReset(DISABLE);
    /* Enable cache */
    EFM_CacheCmd(ENABLE);
}

/**
 * @brief  printf 设备预初始化：USART3 TX (PB7)，写法同 DDL 例程 BSP_PRINTF_Preinit
 */
static int32_t PrintfPreinit(void *vpDevice, uint32_t u32Baudrate)
{
    uint32_t u32Div;
    float32_t f32Error;
    stc_usart_uart_init_t stcUartInit;
    int32_t i32Ret = LL_ERR_INVD_PARAM;

    (void)vpDevice;

    if (0UL != u32Baudrate) {
        GPIO_SetFunc(PRINTF_TX_PORT, PRINTF_TX_PIN, PRINTF_TX_FUNC);
        FCG_Fcg1PeriphClockCmd(PRINTF_USART_FCG, ENABLE);
        (void)USART_UART_StructInit(&stcUartInit);
        stcUartInit.u32OverSampleBit = USART_OVER_SAMPLE_8BIT;
        (void)USART_UART_Init(PRINTF_USART, &stcUartInit, NULL);
        for (u32Div = 0UL; u32Div <= USART_CLK_DIV64; u32Div++) {
            USART_SetClockDiv(PRINTF_USART, u32Div);
            i32Ret = USART_SetBaudrate(PRINTF_USART, u32Baudrate, &f32Error);
            if ((LL_OK == i32Ret) &&
                ((-PRINTF_BAUDRATE_ERR_MAX <= f32Error) && (f32Error <= PRINTF_BAUDRATE_ERR_MAX))) {
                USART_FuncCmd(PRINTF_USART, USART_TX, ENABLE);
                break;
            } else {
                i32Ret = LL_ERR;
            }
        }
    }
    return i32Ret;
}

void PrintResetCause(void)
{
    static const struct {
        uint32_t u32Flag;
        const char *pcszName;
    } stcCause[] = {
        {RMU_FLAG_PWR_ON,         "Power-on reset (POR)"          },
        {RMU_FLAG_PIN,            "Reset-pin reset (NRST)"        },
        {RMU_FLAG_BROWN_OUT,      "Brown-out reset (BOR)"         },
        {RMU_FLAG_PVD1,           "PVD1 reset"                    },
        {RMU_FLAG_PVD2,           "PVD2 reset"                    },
        {RMU_FLAG_WDT,            "WDT reset"                     },
        {RMU_FLAG_SWDT,           "SWDT reset"                    },
        {RMU_FLAG_PWR_DOWN,       "Power-down reset (PDR)"        },
        {RMU_FLAG_SW,             "Software reset"                },
        {RMU_FLAG_MPU_ERR,        "MPU error reset"               },
        {RMU_FLAG_RAM_PARITY_ERR, "RAM parity error reset"        },
        {RMU_FLAG_RAM_ECC,        "RAM ECC error reset"           },
        {RMU_FLAG_CLK_ERR,        "Clock frequency error reset"   },
        {RMU_FLAG_XTAL_ERR,       "XTAL error reset"              },
        {RMU_FLAG_MX,             "Multiple reset cause"          },
    };
    uint32_t u32Rstf0;
    uint32_t u32Idx;
    uint32_t u32Cnt = 0UL;

    /* 原始值先打印，方便核对（只读寄存器，读取不会清除标志） */
    u32Rstf0 = (uint32_t)CM_RMU->RSTF0;
    printf("Reset cause: RSTF0 = 0x%04X\r\n", (unsigned int)u32Rstf0);

    /* 逐个列出所有已置位的复位原因 */
    for (u32Idx = 0UL; u32Idx < (sizeof(stcCause) / sizeof(stcCause[0])); u32Idx++) {
        if (0UL != (u32Rstf0 & stcCause[u32Idx].u32Flag)) {
            printf("  [%u] %s\r\n", (unsigned int)(++u32Cnt), stcCause[u32Idx].pcszName);
        }
    }
    if (0UL == u32Cnt) {
        printf("  (no reset cause flag set)\r\n");
    }

    /* 打印后清除标志（RSTF0 写需 PWC 解锁码1），下次复位后只显示本次新增的原因。
       直接置 CLRF 位，不调用 hc32_ll_rmu.c（工程未编译该文件） */
    PWC_REG_Unlock(PWC_UNLOCK_CODE1);
    SET_REG_BIT(CM_RMU->RSTF0, RMU_RSTF0_CLRF);
    PWC_REG_Lock(PWC_UNLOCK_CODE1);
}

/**
 * @brief  Main function
 */
int32_t main(void)
{
    uint32_t u32Cnt = 0UL;

    /* Disable SysTick interrupt (TICKINT=0) - boot does not need SysTick */
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
    SysTick->CTRL  = 0UL;

    /* MCU Peripheral registers write unprotected */
    LL_PERIPH_WE(LL_PERIPH_SEL);

    /* System clock: MPLL@200MHz */
    SystemClockConfig();

    /* 初始化 printf 设备：USART3 @115200 */
    (void)LL_PrintfInit(PRINTF_USART, PRINTF_BAUDRATE, PrintfPreinit);

    /* 打印复位原因（RSTF0 全部标志，打印后自动清除） */
    PrintResetCause();
    /* MCU Peripheral registers write protected */
    LL_PERIPH_WP(LL_PERIPH_SEL);

    /* 立即喂狗（对照 App/driver main） */
    SWDT_FeedDog();

    for (;;) {
        printf("BOOT alive %u\r\n", (unsigned int)u32Cnt);
        SWDT_FeedDog();
        u32Cnt++;
        {
            volatile uint32_t dly = 2000UL;
            while (dly--) {
                ;
            }
        }
    }
}

/*******************************************************************************
 * EOF
 ******************************************************************************/
