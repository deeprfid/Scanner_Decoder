#include "sysinit.h"


void sysinit(void)
{
    sysinitRCC_Configuration();
    sysinitGPIO_Configuration();
}


static void ClkInit(void)
{
    stc_clk_xtal_cfg_t   stcXtalCfg;
    stc_clk_mpll_cfg_t   stcMpllCfg;
    en_clk_sys_source_t  enSysClkSrc;
    stc_clk_sysclk_cfg_t stcSysClkCfg;
    stc_sram_config_t    stcSramConfig;

    MEM_ZERO_STRUCT(enSysClkSrc);
    MEM_ZERO_STRUCT(stcSysClkCfg);
    MEM_ZERO_STRUCT(stcXtalCfg);
    MEM_ZERO_STRUCT(stcMpllCfg);
    MEM_ZERO_STRUCT(stcSramConfig);

    /* Set bus clk div. */
    stcSysClkCfg.enHclkDiv  = ClkSysclkDiv1;  /* Max 168MHz */
    stcSysClkCfg.enExclkDiv = ClkSysclkDiv2;  /* Max 84MHz */
    stcSysClkCfg.enPclk0Div = ClkSysclkDiv1;  /* Max 168MHz */
    stcSysClkCfg.enPclk1Div = ClkSysclkDiv2;  /* Max 84MHz */
    stcSysClkCfg.enPclk2Div = ClkSysclkDiv4;  /* Max 60MHz */
    stcSysClkCfg.enPclk3Div = ClkSysclkDiv4;  /* Max 42MHz */
    stcSysClkCfg.enPclk4Div = ClkSysclkDiv2;  /* Max 84MHz */
    CLK_SysClkConfig(&stcSysClkCfg);

    /* Switch system clock source to MPLL. */
    /* Use Xtal as MPLL source. */
    stcXtalCfg.enMode = ClkXtalModeOsc;
    stcXtalCfg.enDrv = ClkXtalLowDrv;
    stcXtalCfg.enFastStartup = Enable;
    CLK_XtalConfig(&stcXtalCfg);
    CLK_XtalCmd(Enable);


    /* MPLL config. */  //8M /1 x42 /2=168M
    stcMpllCfg.pllmDiv =1ul;
    stcMpllCfg.plln = 42ul;
    //stcMpllCfg.pllmDiv = 2ul;
    //stcMpllCfg.plln = 42ul;
    //stcMpllCfg.PllpDiv = 4ul;
    //stcMpllCfg.PllqDiv = 4ul;
    //stcMpllCfg.PllrDiv = 4ul;
    stcMpllCfg.PllpDiv = 2ul;
    stcMpllCfg.PllqDiv = 2ul;
    stcMpllCfg.PllrDiv = 2ul;
    CLK_SetPllSource(ClkPllSrcXTAL);
    CLK_MpllConfig(&stcMpllCfg);

    /* flash read wait cycle setting */
    EFM_Unlock();
    EFM_SetLatency(EFM_LATENCY_5);
    EFM_Lock();

    stcSramConfig.u8SramIdx = Sram12Idx | Sram3Idx | SramHsIdx | SramRetIdx;
    stcSramConfig.enSramRC = SramCycle2;
    stcSramConfig.enSramWC = SramCycle2;
    stcSramConfig.enSramEccMode = EccMode0;//不校验ECC
    stcSramConfig.enSramEccOp = SramNmi;
    stcSramConfig.enSramPyOp = SramNmi;
    SRAM_Init(&stcSramConfig);

    /* Enable MPLL. */
    CLK_MpllCmd(Enable);

    /* Wait MPLL ready. */
    while (Set != CLK_GetFlagStatus(ClkFlagMPLLRdy))
    {
        SWDT_RefreshCounter();
    }

    /* Switch system clock source to MPLL. */
    CLK_SetSysClkSource(CLKSysSrcMPLL);
}

void sysinitRCC_Configuration(void)
{
    uint32_t u32Fcg1Periph = PWC_FCG1_PERIPH_USART1 | PWC_FCG1_PERIPH_USART2 | PWC_FCG1_PERIPH_CAN | PWC_FCG1_PERIPH_USBFS |
                             PWC_FCG1_PERIPH_USART3 | PWC_FCG1_PERIPH_USART4;
    ClkInit();
    /* Enable peripheral clock */
    PWC_Fcg1PeriphClockCmd(u32Fcg1Periph, Enable);  //外围功能控制时钟

    /* Enable peripheral clock */
    PWC_Fcg0PeriphClockCmd(PWC_FCG0_PERIPH_DMA1 | PWC_FCG0_PERIPH_DMA2,Enable);


}





void sysinitGPIO_Configuration(void)
{

    stc_port_init_t Port_CFG;
    MEM_ZERO_STRUCT(Port_CFG);
    Port_CFG.enPinMode = Pin_Mode_Out;
    Port_CFG.enPinDrv = Pin_Drv_L;
    PORT_Unlock();//设置JTAG/SWD功能无效
    M4_PORT->PSPCR  = 0x00u;
    PORT_Lock();

    PORT_Init(PortB, Pin02, &Port_CFG);
    M4_PORT->PODRB_f.POUT02 = 0;   //RFID  POW control    0 OFF

    PORT_Init(PortC, Pin13, &Port_CFG);
    M4_PORT->PODRC_f.POUT13 = 0;   //外扩展  POW control  0 OFF

    PORT_Init(PortB, Pin01, &Port_CFG);
    M4_PORT->PODRB_f.POUT01 = 0;   //W5100S reset

    PORT_Init(PortB, Pin03, &Port_CFG);
    M4_PORT->PODRB_f.POUT03 = 1;   //指示灯  1 OFF

    PORT_Init(PortA, Pin10, &Port_CFG);
    M4_PORT->PODRA_f.POUT10 = 0;   //蜂鸣器  0 OFF

    PORT_Init(PortB, Pin15, &Port_CFG);
    M4_PORT->PODRB_f.POUT15 = 0;   // 485 dir control  0 rec

    PORT_Init(PortC, Pin14, &Port_CFG);
    M4_PORT->PODRC_f.POUT14 = 1;   // WG D1
    PORT_Init(PortC, Pin15, &Port_CFG);
    M4_PORT->PODRC_f.POUT15 = 1;   // WG D0

    PORT_Init(PortB, Pin05, &Port_CFG);
    M4_PORT->PODRB_f.POUT05 = 0;   //GPO1
    PORT_Init(PortB, Pin08, &Port_CFG);
    M4_PORT->PODRB_f.POUT08 = 0;   //GPO2
    PORT_Init(PortB, Pin09, &Port_CFG);
    M4_PORT->PODRB_f.POUT09 = 0;   //GPO3
    PORT_Init(PortH, Pin02, &Port_CFG);
    M4_PORT->PODRH_f.POUT02 = 0;   //GPO4

    Port_CFG.enPinMode = Pin_Mode_In;
    PORT_Init(PortA, Pin13, &Port_CFG);//IN1
    PORT_Init(PortA, Pin14, &Port_CFG);//IN2
    PORT_Init(PortA, Pin15, &Port_CFG);//IN3
    PORT_Init(PortB, Pin04, &Port_CFG);//IN4

    PORT_Init(PortA, Pin09, &Port_CFG);//USB-VBUS
    PORT_Init(PortA, Pin11, &Port_CFG);//DM
    PORT_Init(PortA, Pin12, &Port_CFG);//DP

    PORT_Init(PortA, Pin08, &Port_CFG);//IP RESET

    Port_CFG.enPullUp  = Enable;
    PORT_Init(PortB, Pin00, &Port_CFG);//W5100 INT

    PORT_Init(PortA, Pin02, &Port_CFG);//TX1 上拉
    PORT_Init(PortA, Pin03, &Port_CFG);//RX1 上拉
    PORT_Init(PortA, Pin00, &Port_CFG);//TX2 上拉
    PORT_Init(PortA, Pin01, &Port_CFG);//RX2 上拉
    PORT_Init(PortB, Pin07, &Port_CFG);//TX3 上拉
    PORT_Init(PortB, Pin06, &Port_CFG);//RX3 上拉
    PORT_Init(PortB, Pin13, &Port_CFG);//TX4 上拉
    PORT_Init(PortB, Pin14, &Port_CFG);//RX4 上拉


}


