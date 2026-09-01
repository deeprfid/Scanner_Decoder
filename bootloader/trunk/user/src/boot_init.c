/**
 * @file boot_init.c
 * @brief Boot minimal HW init (F460): clock + GPIO + UART2 printf + SysTick
 *        Compiled against FWlib sources directly (no prebuilt driver lib).
 */
#include "hc32_ddl.h"
#include "hc32f460_clk.h"
#include "hc32f460_gpio.h"
#include "hc32f460_sram.h"
#include "hc32f460_efm.h"
#include "hc32f460_pwc.h"
#include "hc32f460_usart.h"
#include <stdio.h>
#include <stdarg.h>

/* ---- SystemCoreClock (CMSIS) ---- */
extern uint32_t SystemCoreClock;

/* ---- Clock: 8M XTAL /1 * 42 /2 = 168MHz MPLL ---- */
static void ClkInit(void)
{
    stc_clk_xtal_cfg_t   stcXtalCfg;
    stc_clk_mpll_cfg_t   stcMpllCfg;
    stc_clk_sysclk_cfg_t stcSysClkCfg;
    stc_sram_config_t    stcSramConfig;

    MEM_ZERO_STRUCT(stcSysClkCfg);
    MEM_ZERO_STRUCT(stcXtalCfg);
    MEM_ZERO_STRUCT(stcMpllCfg);
    MEM_ZERO_STRUCT(stcSramConfig);

    /* bus dividers */
    stcSysClkCfg.enHclkDiv  = ClkSysclkDiv1;  /* 168MHz */
    stcSysClkCfg.enExclkDiv = ClkSysclkDiv2;  /* 84MHz  */
    stcSysClkCfg.enPclk0Div = ClkSysclkDiv1;  /* 168MHz */
    stcSysClkCfg.enPclk1Div = ClkSysclkDiv2;  /* 84MHz  */
    stcSysClkCfg.enPclk2Div = ClkSysclkDiv4;  /* 42MHz  */
    stcSysClkCfg.enPclk3Div = ClkSysclkDiv4;  /* 42MHz  */
    stcSysClkCfg.enPclk4Div = ClkSysclkDiv2;  /* 84MHz  */
    CLK_SysClkConfig(&stcSysClkCfg);

    /* XTAL 8MHz */
    stcXtalCfg.enMode = ClkXtalModeOsc;
    stcXtalCfg.enDrv  = ClkXtalLowDrv;
    stcXtalCfg.enFastStartup = Enable;
    CLK_XtalConfig(&stcXtalCfg);
    CLK_XtalCmd(Enable);

    /* MPLL: 8M/1 * 42 /2 = 168M */
    stcMpllCfg.pllmDiv  = 1ul;
    stcMpllCfg.plln    = 42ul;
    stcMpllCfg.PllpDiv = 2ul;
    stcMpllCfg.PllqDiv = 2ul;
    stcMpllCfg.PllrDiv = 2ul;
    CLK_SetPllSource(ClkPllSrcXTAL);
    CLK_MpllConfig(&stcMpllCfg);

    /* flash wait cycle for 168MHz */
    EFM_Unlock();
    EFM_SetLatency(EFM_LATENCY_5);
    EFM_Lock();

    /* SRAM wait + ECC off */
    stcSramConfig.u8SramIdx    = Sram12Idx | Sram3Idx | SramHsIdx | SramRetIdx;
    stcSramConfig.enSramRC     = SramCycle2;
    stcSramConfig.enSramWC     = SramCycle2;
    stcSramConfig.enSramEccMode = EccMode0;
    stcSramConfig.enSramEccOp  = SramNmi;
    stcSramConfig.enSramPyOp   = SramNmi;
    SRAM_Init(&stcSramConfig);

    /* switch to MPLL */
    CLK_MpllCmd(Enable);
    while (Set != CLK_GetFlagStatus(ClkFlagMPLLRdy))
    {
    }
    CLK_SetSysClkSource(CLKSysSrcMPLL);
    SystemCoreClock = 168000000UL;
}

/* ---- RCC: clock + enable USART2/QSPI ---- */
void RCC_Configuration(void)
{
    uint32_t u32Fcg1 = PWC_FCG1_PERIPH_USART2 | PWC_FCG1_PERIPH_QSPI;
    ClkInit();
    PWC_Fcg1PeriphClockCmd(u32Fcg1, Enable);
}

/* ---- GPIO: unlock + clear PSPCR ---- */
void GPIO_Configuration(void)
{
    PORT_Unlock();
    M4_PORT->PSPCR = 0x00u;
    PORT_Lock();
}

/* ---- SysTick 1ms tick ---- */
void timer_Init(void)
{
    SysTick_Config(SystemCoreClock / 1000u);
}

/* ---- UART2 (PA00 TX @115200) for boot printf ---- */
static void boot_uart_init(void)
{
    stc_usart_uart_init_t stcInitCfg;

    MEM_ZERO_STRUCT(stcInitCfg);

    PORT_SetFunc(PortA, Pin00, Func_Usart2_Tx, Disable);
    PORT_SetFunc(PortA, Pin01, Func_Usart2_Rx, Disable);

    stcInitCfg.enClkMode     = UsartIntClkCkNoOutput;
    stcInitCfg.enClkDiv      = UsartClkDiv_1;
    stcInitCfg.enDataLength  = UsartDataBits8;
    stcInitCfg.enDirection   = UsartDataLsbFirst;
    stcInitCfg.enStopBit     = UsartOneStopBit;
    stcInitCfg.enParity      = UsartParityNone;
    stcInitCfg.enSampleMode  = UsartSampleBit8;
    stcInitCfg.enDetectMode  = UsartStartBitFallEdge;
    stcInitCfg.enHwFlow      = UsartRtsEnable;
    (void)USART_UART_Init(M4_USART2, &stcInitCfg);
    (void)USART_SetBaudrate(M4_USART2, 115200UL);
    (void)USART_FuncCmd(M4_USART2, UsartRx, Enable);
    (void)USART_FuncCmd(M4_USART2, UsartTx, Enable);
}

/* ---- boot printf: polled UART2 output ---- */
void boot_putc(char c)
{
    while (Set != USART_GetStatus(M4_USART2, UsartTxEmpty))
    {
    }
    (void)USART_SendData(M4_USART2, (uint16_t)(uint8_t)c);
}

void boot_printf(const char *fmt, ...)
{
    char buf[160];
    va_list ap;
    char *p;
    va_start(ap, fmt);
    (void)vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    for (p = buf; *p != '\0'; p++)
        boot_putc(*p);
}

/* ---- boot entry: full hw init ---- */
void boot_hw_init(void)
{
    RCC_Configuration();
    GPIO_Configuration();
    timer_Init();
    boot_uart_init();
}

