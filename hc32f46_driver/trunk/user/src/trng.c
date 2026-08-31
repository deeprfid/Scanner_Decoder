/**
 *******************************************************************************
 * @file  trng/trng_base/source/main.c
 * @brief Main program TRNG base for the Device Driver Library.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2023-09-30       CDT             Set XTAL as system clock source
 @endverbatim
 *******************************************************************************
 * Copyright (C) 2022-2023, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 *
 * This software component is licensed by XHSC under BSD 3-Clause license
 * (the "License"); You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                    opensource.org/licenses/BSD-3-Clause
 *
 *******************************************************************************
 */

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "hc32_ddl.h"
#include "hc32f46_driver.h"

/* TRNG clock selection definition. */
#define TRNG_CLK_PCLK4              (1u)
#define TRNG_CLK_MPLLQ              (2u)
#define TRNG_CLK_UPLLR              (3u)

/* Select MPLLQ as TRNG clock. */
#define TRNG_CLK                    (TRNG_CLK_MPLLQ)
#define TIMEOUT_10MS                (1u)

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/
 

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @brief  Main function of template project
 * @param  None
 * @retval int32_t return value, if needed
 */
void trng_create(uint8_t *trngbuf,uint8_t u8Length)
{
   
    /* Unlock peripherals or registers */

    /* Lock peripherals or registers */
 
    (void)TRNG_Generate((uint32_t*)trngbuf, u8Length/4U,TIMEOUT_10MS);
    
    //memcpy(inbuf,(uint8_t*)&m_au32Random,8);
    
    return ;

}

static void TrngClockConfig(void)
{
#if (TRNG_CLK == TRNG_CLK_PCLK4)
    /* PCLK4 is TRNG's clock. */
    /* TRNG's clock frequency below 1MHz(inclusive) if possible will be better. */
    m_stcSysclkCfg.enPclk4Div = ClkSysclkDiv64;
    CLK_SysClkConfig(&m_stcSysclkCfg);
    CLK_SetPeriClkSource(ClkPeriSrcPclk);

#elif (TRNG_CLK == TRNG_CLK_MPLLQ)
    stc_clk_xtal_cfg_t stcXtalCfg;
    stc_clk_mpll_cfg_t stcMpllCfg;
    en_clk_sys_source_t enSysClkSrc;

    enSysClkSrc = CLK_GetSysClkSource();
    if (enSysClkSrc == CLKSysSrcMPLL)
    {
        /*
         * Configure MPLLQ(same as MPLLP and MPLLR) when you
         * configure MPLL as the system clock.
         */
    }
    else
    {
        /* Use XTAL as MPLL source. */
        stcXtalCfg.enFastStartup = Enable;
        stcXtalCfg.enMode = ClkXtalModeOsc;
        stcXtalCfg.enDrv  = ClkXtalLowDrv;
        CLK_XtalConfig(&stcXtalCfg);
        CLK_XtalCmd(Enable);

        /* Set MPLL out 240MHz. */
        stcMpllCfg.pllmDiv = 1u;
        /* mpll = 8M / pllmDiv * plln */
        stcMpllCfg.plln    = 30u;
        stcMpllCfg.PllpDiv = 2u;
        stcMpllCfg.PllqDiv = 16u;
        stcMpllCfg.PllrDiv = 4u;
        CLK_SetPllSource(ClkPllSrcXTAL);
        CLK_MpllConfig(&stcMpllCfg);
        CLK_MpllCmd(Enable);
    }

    CLK_SetPeriClkSource(ClkPeriSrcMpllp);

#elif (TRNG_CLK == TRNG_CLK_UPLLR)
    stc_clk_xtal_cfg_t stcXtalCfg;
    stc_clk_upll_cfg_t stcUpllCfg;

    MEM_ZERO_STRUCT(stcXtalCfg);
    MEM_ZERO_STRUCT(stcSysclkCfg);

    /* Use XTAL as UPLL source. */
    stcXtalCfg.enFastStartup = Enable;
    stcXtalCfg.enMode = ClkXtalModeOsc;
    stcXtalCfg.enDrv  = ClkXtalLowDrv;
    CLK_XtalConfig(&stcXtalCfg);
    CLK_XtalCmd(Enable);

    /* Set UPLL out 240MHz. */
    stcUpllCfg.pllmDiv = 2u;
    /* upll = 8M(XTAL) / pllmDiv * plln */
    stcUpllCfg.plln    = 60u;
    stcUpllCfg.PllpDiv = 2u;
    stcUpllCfg.PllqDiv = 16u;
    stcUpllCfg.PllrDiv = 16u;
    CLK_SetPllSource(ClkPllSrcXTAL);
    CLK_UpllConfig(&stcUpllCfg);
    CLK_UpllCmd(Enable);
    CLK_SetPeriClkSource(ClkPeriSrcUpllr);
#endif
}
/**
 * @brief  TRNG initialization configuration.
 * @param  None
 * @retval None
 */
 void TrngInitConfig(void)
{
    stc_trng_init_t stcTrngInit;

    TrngClockConfig();
    stcTrngInit.enLoadCtrl   = TrngLoadNewInitValue_Enable;
    stcTrngInit.enShiftCount = TrngShiftCount_64;

    /* 1. Enable TRNG. */
    PWC_Fcg0PeriphClockCmd(PWC_FCG0_PERIPH_TRNG, Enable);
    /* 2. Initialize TRNG. */
    TRNG_Init(&stcTrngInit);
}

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
