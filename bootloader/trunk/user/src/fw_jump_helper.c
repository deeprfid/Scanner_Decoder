#include "hc32_common.h"
#include "hc32f460_usart.h"
#include "hc32f460_timera.h"
#include "hc32f460_efm.h"
#include "hc32f460_interrupts.h"
#include "hc32f46_driver.h"
#include "fw_jump_helper.h"


void EFM_SetWaitCycle(uint32_t u32Cycle)
{
    uint32_t u32Temp;

    /* Set flash wait cycle. */
    /* Unlock flash. */
    EFM_Unlock();
    /* Set wait cycle. */
    u32Temp  = M4_EFM->FRMC;
    u32Temp &= ~(0xFul << 4u);
    u32Temp |= (u32Cycle << 4u);
    M4_EFM->FRMC = u32Temp;
    /* Lock flash. */
    EFM_Lock();
}


void SystemClock_DeInit(void)
{
    uint32_t u32Timeout = 0u;

    /* Unlock CMU. */
    M4_SYSREG->PWR_FPRC = 0xa501;

    /* Close fcg0~fcg3. */
    M4_MSTP->FCG0 = 0xFFFFFAEE;
    M4_MSTP->FCG1 = 0xFFFFFFFF;
    M4_MSTP->FCG2 = 0xFFFFFFFF;
    M4_MSTP->FCG3 = 0xFFFFFFFF;

    /* Wait stable after close fcg. */
    u32Timeout = (0x1000ul);
    while (u32Timeout--);

    M4_SYSREG->CMU_CKSWR =  ((uint8_t)0x1);

    u32Timeout = (0x1000ul);
    while (u32Timeout--);

    /* Set CMU registers to default value. */
#if (SYSTEM_CLOCK_SOURCE == CLK_SOURCE_XTAL)
    M4_SYSREG->CMU_XTALCFGR = (uint8_t)0x00;
    M4_SYSREG->CMU_XTALCR   = (uint8_t)0x01;
#else
    M4_SYSREG->CMU_HRCCR    = (uint8_t)0x01;
#endif // #if (SYSTEM_CLOCK_SOURCE == CLK_SOURCE_XTAL)
    M4_SYSREG->CMU_PLLCFGR  = (uint32_t)0x11101300;
    M4_SYSREG->CMU_PLLCR    = (uint8_t)0x01;
    M4_SYSREG->CMU_SCFGR    = (uint32_t)0x00;

    u32Timeout = (0x1000ul);
    while (u32Timeout--);

    EFM_SetWaitCycle(0x0u);

    u32Timeout =(0x1000ul);
    while (u32Timeout--);

    /* Lock CMU. */
    M4_SYSREG->PWR_FPRC = 0xa500;
}


void IAP_ResetConfig(void)
{
	__disable_irq();
    USART_FuncCmd(M4_USART1, UsartRx, Disable);
    USART_FuncCmd(M4_USART1, UsartRxInt, Disable);//不使能接收中断
    USART_FuncCmd(M4_USART2, UsartRx, Disable);
    USART_FuncCmd(M4_USART2, UsartRxInt, Disable);//不使能接收中断
    USART_FuncCmd(M4_USART3, UsartRx, Disable);
    USART_FuncCmd(M4_USART3, UsartRxInt, Disable);//不使能接收中断
    USART_FuncCmd(M4_USART4, UsartRx, Disable);
    USART_FuncCmd(M4_USART4, UsartRxInt, Disable);//不使能接收中断

    TIMERA_Cmd(M4_TMRA1, Disable);
    TIMERA_Cmd(M4_TMRA2, Disable);
    TIMERA_Cmd(M4_TMRA3, Disable);
	TIMERA_Cmd(M4_TMRA4, Disable);
    TIMERA_IrqCmd(M4_TMRA4, TimeraIrqOverflow, Disable);

    enIrqResign(Int000_IRQn);  //释放中断
    enIrqResign(Int001_IRQn);
    enIrqResign(Int002_IRQn);
    enIrqResign(Int003_IRQn);
    enIrqResign(Int004_IRQn);
    enIrqResign(Int005_IRQn);
    enIrqResign(Int006_IRQn);
    enIrqResign(Int007_IRQn);
    enIrqResign(Int008_IRQn);
    enIrqResign(Int009_IRQn);
	 enIrqResign(Int024_IRQn);
    // UART_DeInit();
    SystemClock_DeInit();
}

void run_app(uint32_t appAddr)
{
	uint32_t ApplicationAddress = appAddr;
	uint32_t JumpAddress;
	func_ptr_t JumpToApplication;
   uint32_t u32StackTop = *((__IO uint32_t *)ApplicationAddress);
    /* Check if user code is programmed starting from address "u32Addr" */
    /* Check stack top pointer. 第一个地址是堆栈值 看有没有超出范围 */
    if ((u32StackTop > SRAM_BASE) && (u32StackTop <= (SRAM_BASE + RAM_SIZE)))
    {
			TRACE("start run_app\n");
        IAP_ResetConfig();
        /* Jump to user application */
        JumpAddress = *(__IO uint32_t *)(ApplicationAddress + 4);
        JumpToApplication = (func_ptr_t)JumpAddress;
        /* Initialize user application's Stack Pointer */
        __set_MSP(*(__IO uint32_t *)ApplicationAddress);
        JumpToApplication();
    }
}


uint8 gPageBuffer[PAGE_SIZE];

