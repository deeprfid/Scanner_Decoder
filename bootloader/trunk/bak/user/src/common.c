
#include "common.h"
#include "irq.h"

//2015-1-6------------------
extern uint32_t Main_BpsVal;
uint32_t JumpAddress;
func_ptr_t JumpToApplication;




unsigned char Write_Page(unsigned int Page_Number,unsigned int wordaddr,unsigned int *Buffer,unsigned int len,unsigned char pagestart,unsigned char pageend)
{

    unsigned int i;
    en_efm_status_t enStatus;
    __IO uint32_t *io32Flash = (uint32_t *)wordaddr;
    uint32_t  add = wordaddr;
    EFM_Unlock();
    enStatus = EFM_WaitForOperationDone(1000);
    if (enStatus != EfmOk)
    {   EFM_Lock();
        return enStatus;
    }
    M4_EFM->FSCLR = (uint32_t)0x3F;
    M4_EFM->FWMC_f.PEMODE = 0x1u;
    M4_EFM->FWMC_f.PEMOD  = SingleProgramRB;

    for (i = 0u; i <len; i++)
    {
        if((add)%PAGE_SIZE==0)
        {
            EFM_EraseSector(add);
            EFM_Unlock(); //调用EFM_SectorErase 会lock FLASH
            enStatus = EFM_WaitForOperationDone(1000);
            if (enStatus != EfmOk)
            {
                M4_EFM->FWMC_f.PEMOD  = ReadOnly1;
                M4_EFM->FWMC_f.PEMODE = 0x0u;
                EFM_Lock();
                return enStatus;
            }
            M4_EFM->FSCLR = (uint32_t)0x3F;
            M4_EFM->FWMC_f.PEMODE = 0x1u;
            M4_EFM->FWMC_f.PEMOD  = SingleProgramRB;
        }
        *io32Flash = Buffer[i];
        while(1 != M4_EFM->FSR_f.RDY);
        if (1 == M4_EFM->FSR_f.PGMISMTCH)
        {
            enStatus = EfmError;
            M4_EFM->FWMC_f.PEMOD  = ReadOnly1;
            M4_EFM->FWMC_f.PEMODE = 0x0u;
            EFM_Lock();
            return enStatus;
        }
        io32Flash++;
        add+=4;
        M4_EFM->FSCLR |= (uint32_t)EFM_FLAG_EOP;
    }

    /* Set flash read only. */
    M4_EFM->FWMC_f.PEMOD  = ReadOnly1;
    M4_EFM->FWMC_f.PEMODE = 0x0u;

    EFM_Lock();
    return EfmOk;

}




void Read_Page(unsigned int readaddr,unsigned int *Buffer,unsigned int len)
{

    unsigned int  *Flash_Address;
    unsigned int i,j=0;
    EFM_Unlock();
    Flash_Address = ( unsigned int  * )(readaddr&0xffffffff);
    //每次读是一个字，四个字节
    for( i = 0; i < len; i++ )
    {
        Buffer[ i ] = *( Flash_Address + j )  ;
        j+=1;
    }
    EFM_Lock();
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
    //  stcMpllCfg.pllmDiv = 2ul;
    //  stcMpllCfg.plln = 42ul;
//    stcMpllCfg.PllpDiv = 4ul;
//    stcMpllCfg.PllqDiv = 4ul;
//    stcMpllCfg.PllrDiv = 4ul;
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
    }

    /* Switch system clock source to MPLL. */
    CLK_SetSysClkSource(CLKSysSrcMPLL);
}



void RCC_Configuration(void)
{
    uint32_t u32Fcg1Periph = PWC_FCG1_PERIPH_USART1 | PWC_FCG1_PERIPH_USART2 | PWC_FCG1_PERIPH_CAN | PWC_FCG1_PERIPH_USBFS |
                             PWC_FCG1_PERIPH_USART3 | PWC_FCG1_PERIPH_USART4;
    ClkInit();
    /* Enable peripheral clock */
    PWC_Fcg1PeriphClockCmd(u32Fcg1Periph, Enable);  //外围功能控制时钟

    /* Enable peripheral clock */
    PWC_Fcg0PeriphClockCmd(PWC_FCG0_PERIPH_DMA1 | PWC_FCG0_PERIPH_DMA2,Enable);
}



void UART_DeInit(void)
{
    /* UART Port configure */
    M4_PORT->PWPR = 0xA501;
    /* usart3_tx gpio  */
    M4_PORT->PFSRB7_f.FSEL = 0;
    /* usart3_rx gpio  */
    M4_PORT->PFSRB6_f.FSEL = 0;
    M4_PORT->PWPR = 0xA500;

    M4_USART3->SR  = (uint32_t)0xC0;
    M4_USART3->BRR = (uint32_t)0xFFFF;
    M4_USART3->CR1 = (uint32_t)0x8000000;
    M4_USART3->CR2 = (uint32_t)0x0;
    M4_USART3->CR3 = (uint32_t)0x0;
    M4_USART3->PR  = (uint32_t)0x0;

    /* Enable USART Ch.3 configuration. */
    M4_MSTP->FCG1_f.USART3 = 1u;
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

    enIrqResign(Int000_IRQn);  //释放中断
    enIrqResign(Int001_IRQn);
    enIrqResign(Int002_IRQn);
    enIrqResign(Int003_IRQn);
    enIrqResign(Int004_IRQn);
    // UART_DeInit();
    SystemClock_DeInit();
}


void  run_app(void)
{
    uint32_t u32StackTop = *((__IO uint32_t *)ApplicationAddress);
    /* Check if user code is programmed starting from address "u32Addr" */
    /* Check stack top pointer. 第一个地址是堆栈值 看有没有超出范围 */
    if ((u32StackTop > SRAM_BASE) && (u32StackTop <= (SRAM_BASE + RAM_SIZE)))
    {
			printf("start run_app\n");
        IAP_ResetConfig();
        /* Jump to user application */
        JumpAddress = *(__IO uint32_t *)(ApplicationAddress + 4);
        JumpToApplication = (func_ptr_t)JumpAddress;
        /* Initialize user application's Stack Pointer */
        __set_MSP(*(__IO uint32_t *)ApplicationAddress);
        JumpToApplication();
    }

}



void USART3_IT_ENABLE(void)
{
    USART_FuncCmd(M4_USART3, UsartRxInt, Enable);//使能接收中断
}

void USART3_IT_DISABLE(void)
{
    USART_FuncCmd(M4_USART3, UsartRxInt, Disable);//不使能接收中断
}





void CmdRespone(uint8_t  cmdval,uint16_t resstate,uint8_t datlen,uint8_t *datval)
{
    uint8_t dat[256]= {0xff,0X00,0x00,0x00,0x00,0x00,0x00},i=0;
    uint16_t crcval=0;
    if(datlen==0)
    {
        dat[2]=cmdval;
        dat[3]=(resstate&0xff00)>>8;
        dat[4]=resstate&0xff;
        crcval=crc_CalcCRC(dat,5);
        dat[5]=crcval>>8;
        dat[6]=crcval;
        for(i=0; i<7; i++)
        {
            USART_SendData(M4_USART3, dat[i]);
        }
    }
    else
    {
        dat[1]=datlen;
        dat[2]=cmdval;
        dat[3]=(resstate&0xff00)>>8;
        dat[4]=resstate&0xff;
        for(i=0; i<datlen; i++)
        {
            dat[5+i]=datval[i];
        }
        crcval=crc_CalcCRC(dat,(5+datlen));
        dat[5+datlen]=crcval>>8;
        dat[6+datlen]=crcval;
        for(i=0; i<(7+datlen); i++)
        {
            USART_SendData(M4_USART3, dat[i]);
        }
    }
}



void  GPIO_Configuration(void)
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

