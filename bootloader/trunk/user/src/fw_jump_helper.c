#include "hc32_common.h"
#include "hc32f460_efm.h"
#include "fw_jump_helper.h"

/* boot_init.c */
void boot_printf(const char *fmt, ...);

/* ---- local trace: boot_printf (UART2) ---- */
#define TRACE boot_printf

void EFM_SetWaitCycle(uint32_t u32Cycle)
{
    uint32_t u32Temp;

    /* Set flash wait cycle. */
    EFM_Unlock();
    u32Temp  = M4_EFM->FRMC;
    u32Temp &= ~(0xFul << 4u);
    u32Temp |= (u32Cycle << 4u);
    M4_EFM->FRMC = u32Temp;
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

    u32Timeout = (0x1000ul);
    while (u32Timeout--);

    M4_SYSREG->CMU_CKSWR = ((uint8_t)0x1);

    u32Timeout = (0x1000ul);
    while (u32Timeout--);

    /* Set CMU registers to default value. */
    M4_SYSREG->CMU_HRCCR = (uint8_t)0x01;
    M4_SYSREG->CMU_PLLCFGR = (uint32_t)0x11101300;
    M4_SYSREG->CMU_PLLCR = (uint8_t)0x01;
    M4_SYSREG->CMU_SCFGR = (uint32_t)0x00;

    u32Timeout = (0x1000ul);
    while (u32Timeout--);

    EFM_SetWaitCycle(0x0u);

    u32Timeout = (0x1000ul);
    while (u32Timeout--);

    /* Lock CMU. */
    M4_SYSREG->PWR_FPRC = 0xa500;
}

/* ---- Reset peripheral config before jumping to app (register level) ---- */
void IAP_ResetConfig(void)
{
    __disable_irq();
    SystemClock_DeInit();
}

void run_app(uint32_t appAddr)
{
    uint32_t ApplicationAddress = appAddr;
    uint32_t JumpAddress;
    func_ptr_t JumpToApplication;
    uint32_t u32StackTop = *((__IO uint32_t *)ApplicationAddress);

    /* Check stack top pointer is inside SRAM */
    if ((u32StackTop > SRAM_BASE) && (u32StackTop <= (SRAM_BASE + RAM_SIZE)))
    {
        TRACE("start run_app\n");
        IAP_ResetConfig();
        JumpAddress = *(__IO uint32_t *)(ApplicationAddress + 4);
        JumpToApplication = (func_ptr_t)JumpAddress;
        __set_MSP(*(__IO uint32_t *)ApplicationAddress);
        JumpToApplication();
    }
    /* Invalid stack: stay here (watchdog / manual reset) */
    for (;;)
    {
    }
}

uint8_t gPageBuffer[PAGE_SIZE];



