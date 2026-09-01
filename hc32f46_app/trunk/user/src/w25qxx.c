/*******************************************************************************
 * Copyright (C) 2016, Huada Semiconductor Co., Ltd. All rights reserved.
 *
 * This software is owned and published by:
 * Huada Semiconductor Co., Ltd. ("HDSC").
 *
 * BY DOWNLOADING, INSTALLING OR USING THIS SOFTWARE, YOU AGREE TO BE BOUND
 * BY ALL THE TERMS AND CONDITIONS OF THIS AGREEMENT.
 *
 * This software contains source code for use with HDSC
 * components. This software is licensed by HDSC to be adapted only
 * for use in systems utilizing HDSC components. HDSC shall not be
 * responsible for misuse or illegal use of this software for devices not
 * supported herein. HDSC is providing this software "AS IS" and will
 * not be responsible for issues arising from incorrect user implementation
 * of the software.
 *
 * Disclaimer:
 * HDSC MAKES NO WARRANTY, EXPRESS OR IMPLIED, ARISING BY LAW OR OTHERWISE,
 * REGARDING THE SOFTWARE (INCLUDING ANY ACCOMPANYING WRITTEN MATERIALS),
 * ITS PERFORMANCE OR SUITABILITY FOR YOUR INTENDED USE, INCLUDING,
 * WITHOUT LIMITATION, THE IMPLIED WARRANTY OF MERCHANTABILITY, THE IMPLIED
 * WARRANTY OF FITNESS FOR A PARTICULAR PURPOSE OR USE, AND THE IMPLIED
 * WARRANTY OF NONINFRINGEMENT.
 * HDSC SHALL HAVE NO LIABILITY (WHETHER IN CONTRACT, WARRANTY, TORT,
 * NEGLIGENCE OR OTHERWISE) FOR ANY DAMAGES WHATSOEVER (INCLUDING, WITHOUT
 * LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION,
 * LOSS OF BUSINESS INFORMATION, OR OTHER PECUNIARY LOSS) ARISING FROM USE OR
 * INABILITY TO USE THE SOFTWARE, INCLUDING, WITHOUT LIMITATION, ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL OR CONSEQUENTIAL DAMAGES OR LOSS OF DATA,
 * SAVINGS OR PROFITS,
 * EVEN IF Disclaimer HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * YOU ASSUME ALL RESPONSIBILITIES FOR SELECTION OF THE SOFTWARE TO ACHIEVE YOUR
 * INTENDED RESULTS, AND FOR THE INSTALLATION OF, USE OF, AND RESULTS OBTAINED
 * FROM, THE SOFTWARE.
 *
 * This software may be replicated in part or whole for the licensed use,
 * with the restriction that this Disclaimer and Copyright notice must be
 * included with each copy of this software, whether used in part or whole,
 * at all times.
 */
/******************************************************************************/
/** \file w25qxx.c
 **
 ** A detailed description is available at
 ** @link W25QXXGroup W25Q64 description @endlink
 **
 **   - 2019-05-15  1.0 Zhangxl First version for Device Driver Library of W25QXX.
 **
 ******************************************************************************/

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "hc32_ddl.h"
#include "w25qxx.h"

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
/* QSPCK Port/Pin definition */
#define QSPCK_PORT                      (PortB)
#define QSPCK_PIN                       (Pin14)

/* QSNSS Port/Pin definition */
#define QSNSS_PORT                      (PortB)
#define QSNSS_PIN                       (Pin01)

/* QSIO0 Port/Pin definition */
#define QSIO0_PORT                      (PortB)
#define QSIO0_PIN                       (Pin13)

/* QSIO1 Port/Pin definition */
#define QSIO1_PORT                      (PortB)
#define QSIO1_PIN                       (Pin12)

#define QSIO2_PORT                      (PortB)
#define QSIO2_PIN                       (Pin10)

#define QSIO3_PORT                      (PortB)
#define QSIO3_PIN                       (Pin02)

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/
uint16_t W25QXX_TYPE = W25Q64;
uint8_t  W25QXX_BUFFER[4096];

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/
/**
 *******************************************************************************
 ** \brief W25QXX IO initialize
 **
 ** \param  None
 **
 ** \retval None
 **
 ******************************************************************************/
void W25QXX_Init(void)
{
    stc_qspi_init_t stcQspiInit;

    /* configuration structure initialization */
    MEM_ZERO_STRUCT(stcQspiInit);

    stc_port_init_t Port_CFG;

    /* Configuration peripheral clock */
    PWC_Fcg1PeriphClockCmd(PWC_FCG1_PERIPH_QSPI, Enable);

    /* QSPI 引脚高驱动（对齐 SDK：高速时钟需 PIN_HIGH_DRV） */
    MEM_ZERO_STRUCT(Port_CFG);
    Port_CFG.enPinMode  = Pin_Mode_In;
    Port_CFG.enPinDrv   = Pin_Drv_H;   /* mux pin: drive only, func by PORT_SetFunc */
    PORT_Init(QSPCK_PORT, QSPCK_PIN, &Port_CFG);
    PORT_Init(QSNSS_PORT, QSNSS_PIN, &Port_CFG);
    PORT_Init(QSIO0_PORT, QSIO0_PIN, &Port_CFG);
    PORT_Init(QSIO1_PORT, QSIO1_PIN, &Port_CFG);
    PORT_Init(QSIO2_PORT, QSIO2_PIN, &Port_CFG);
    PORT_Init(QSIO3_PORT, QSIO3_PIN, &Port_CFG);

    /* Configuration QSPI pin */
    PORT_SetFunc(QSPCK_PORT, QSPCK_PIN, Func_Qspi, Disable);
    PORT_SetFunc(QSNSS_PORT, QSNSS_PIN, Func_Qspi, Disable);
    PORT_SetFunc(QSIO0_PORT, QSIO0_PIN, Func_Qspi, Disable);
    PORT_SetFunc(QSIO1_PORT, QSIO1_PIN, Func_Qspi, Disable);
	  PORT_SetFunc(QSIO2_PORT, QSIO2_PIN, Func_Qspi, Disable);
    PORT_SetFunc(QSIO3_PORT, QSIO3_PIN, Func_Qspi, Disable);

    /* Configuration QSPI structure */
    stcQspiInit.enClkDiv = QspiHclkDiv4;   /* 168MHz/4=42MHz，W25Q64 标准读限 50MHz（原 DIV2=84MHz 超限读不到） */
    stcQspiInit.enSpiMode = QspiSpiMode3;   /* AN: SPI Mode 3 */
    stcQspiInit.enBusCommMode = QspiBusModeRomAccess;
    stcQspiInit.enPrefetchMode = QspiPrefetchStopComplete;
    stcQspiInit.enPrefetchFuncEn = Disable;
    stcQspiInit.enQssnValidExtendTime = QspiQssnValidExtendSck32;   /* AN */
    stcQspiInit.enQssnIntervalTime = QspiQssnIntervalQsck8;
    stcQspiInit.enQsckDutyCorr = QspiQsckDutyCorrHalfHclk;   /* AN */
    stcQspiInit.enVirtualPeriod = QspiVirtualPeriodQsck6;   /* AN */
    stcQspiInit.enWpPinLevel = QspiWpPinOutputHigh;   /* AN: WP disable */
    stcQspiInit.enQssnSetupDelayTime = QspiQssnSetupDelay1Dot5Qsck;   /* AN */
    stcQspiInit.enQssnHoldDelayTime = QspiQssnHoldDelay1Dot5Qsck;   /* AN */
    stcQspiInit.enFourByteAddrReadEn = Disable;
    stcQspiInit.enAddrWidth = QspiAddressByteThree;
    stcQspiInit.stcCommProtocol.enReadMode = QspiReadModeStandard;
    stcQspiInit.stcCommProtocol.enTransInstrProtocol = QspiProtocolFourWiresSpi;
    stcQspiInit.stcCommProtocol.enTransAddrProtocol  = QspiProtocolFourWiresSpi;
    stcQspiInit.stcCommProtocol.enReceProtocol       = QspiProtocolFourWiresSpi;
    stcQspiInit.u8RomAccessInstr = QSPI_3BINSTR_STANDARD_READ;
    QSPI_Init(&stcQspiInit);
    /* Read Flash ID */
    W25QXX_TYPE = W25QXX_ReadID();
}

/**
 *******************************************************************************
 ** \brief W25QXX Read Status Register
 **
 ** \param  None
 **
 ** \retval SR value
 **
 ******************************************************************************/
uint8_t W25QXX_ReadSR(void)
{
    uint8_t regSta = 0u;

    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(W25X_ReadStatusReg);
    regSta = QSPI_ReadDirectCommValue();
    QSPI_ExitDirectCommMode();

    return regSta;
}

/**
 *******************************************************************************
 ** \brief W25QXX Write Status Register
 **
 ** \param [in] sr
 **
 ** \retval None
 **
 ******************************************************************************/
void W25QXX_Write_SR(uint8_t sr)
{
    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(W25X_WriteStatusReg);
    QSPI_WriteDirectCommValue(sr);
    QSPI_ExitDirectCommMode();
}

/**
 *******************************************************************************
 ** \brief W25QXX Write enable
 **
 ** \param  None
 **
 ** \retval None
 **
 ******************************************************************************/
void W25QXX_Write_Enable(void)
{
    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(W25X_WriteEnable);
    QSPI_ExitDirectCommMode();
}

/**
 *******************************************************************************
 ** \brief W25QXX Write disable
 **
 ** \param  None
 **
 ** \retval None
 **
 ******************************************************************************/
void W25QXX_Write_Disable(void)
{
    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(W25X_WriteDisable);
    QSPI_ExitDirectCommMode();
}

/**
 *******************************************************************************
 ** \brief W25QXX read flash ID
 **
 ** \param  None
 **
 ** \retval u16FlashID
 **  0XEF13: W25Q80
 **  0XEF14: W25Q16
 **  0XEF15: W25Q32
 **  0XEF16: W25Q64
 **  0XEF17: W25Q128
 **
 ******************************************************************************/
uint16_t W25QXX_ReadID(void)
{
    uint16_t u16FlashID = 0u;

    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(W25X_ManufactDeviceID);
    QSPI_WriteDirectCommValue(0x00u);
    QSPI_WriteDirectCommValue(0x00u);
    QSPI_WriteDirectCommValue(0x00u);
    u16FlashID |= (uint16_t)((uint16_t)QSPI_ReadDirectCommValue() << (int8_t)8);
    u16FlashID |= QSPI_ReadDirectCommValue();
    QSPI_ExitDirectCommMode();
    return u16FlashID;
}

/**
 *******************************************************************************
 ** \brief W25QXX read flash content
 **
 ** \param [in] ReadAddr                Address to be read
 ** \param [in] NumByteToRead           Number to be read, (MAX. 65535)
 ** \param [out] pBuffer                Read data buffer
 **
 ** \retval None
 **
 ******************************************************************************/
void W25QXX_Read(uint8_t* pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead)
{
    uint16_t i;

    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(W25X_ReadData);
    QSPI_WriteDirectCommValue((uint8_t)((ReadAddr) >> 16u));
    QSPI_WriteDirectCommValue((uint8_t)((ReadAddr) >> 8u));
    QSPI_WriteDirectCommValue((uint8_t)ReadAddr);
    for (i = 0u; i < NumByteToRead; i++)
    {
        pBuffer[i] = QSPI_ReadDirectCommValue();
    }
    QSPI_ExitDirectCommMode();
}

/**
 *******************************************************************************
 ** \brief W25QXX page write
 **
 ** \param [in] pBuffer                 data buffer to be written
 ** \param [in] WriteAddr               Address to be written
 ** \param [in] NumByteToWrite          Number to be written, (MAX. 256)
 **
 ** \retval None
 **
 ******************************************************************************/
void W25QXX_Write_Page(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
    W25QXX_Write_Enable();
    /* Send data to flash */
    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(W25X_PageProgram);
    QSPI_WriteDirectCommValue((uint8_t)((WriteAddr & 0xFF0000ul) >> 16u));
    QSPI_WriteDirectCommValue((uint8_t)((WriteAddr & 0xFF00u) >> 8u));
    QSPI_WriteDirectCommValue((uint8_t)(WriteAddr & 0xFFu));
    while (NumByteToWrite--)
    {
        QSPI_WriteDirectCommValue(*pBuffer++);
    }
    QSPI_ExitDirectCommMode();
    /* Wait for flash idle */
    W25QXX_Wait_Busy();
}

/**
 *******************************************************************************
 ** \brief W25QXX flash write
 **
 ** \param [in] pBuffer                 data buffer to be written
 ** \param [in] WriteAddr               Address to be written
 ** \param [in] NumByteToWrite          Number to be written, (MAX. 65535)
 **
 ** \retval None
 **
 ******************************************************************************/
void W25QXX_Write_NoCheck(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
    uint16_t pageremain;
    pageremain = (uint16_t)(256u - WriteAddr % 256u);
    if (NumByteToWrite <= pageremain)
    {
        pageremain = NumByteToWrite;
    }
    while (1)
    {
        W25QXX_Write_Page(pBuffer, WriteAddr, pageremain);
        if (NumByteToWrite == pageremain)
        {
            break;
        }
        else //NumByteToWrite>pageremain
        {
            pBuffer        += pageremain;
            WriteAddr      += pageremain;

            NumByteToWrite -= pageremain;
            if (NumByteToWrite > 256u)
            {
                pageremain = 256u;
            }
            else
            {
                pageremain = NumByteToWrite;
            }
        }
    }
}

/**
 *******************************************************************************
 ** \brief W25QXX flash write API for MSC
 **
 ** \param [in] pBuffer                 data buffer to be written
 ** \param [in] WriteAddr               Address to be written
 ** \param [in] NumByteToWrite          Number to be written, (MAX. 65535)
 **
 ** \retval None
 **
 ******************************************************************************/
void W25QXX_Write(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
    uint32_t secpos;
    uint16_t secoff;
    uint16_t secremain;
    uint16_t i;
    uint8_t * W25QXX_BUF;
    W25QXX_BUF = W25QXX_BUFFER;
    secpos     = WriteAddr / 4096u;
    secoff     = (uint16_t)(WriteAddr % 4096u);
    secremain  = 4096u - secoff;
//    printf("ad:%X,nb:%X\r\n",WriteAddr,NumByteToWrite);           // for test
    if (NumByteToWrite <= secremain)
    {
        secremain = NumByteToWrite;                                //銆�less than 4K
    }
    while (1)
    {
        W25QXX_Read(W25QXX_BUF, secpos * 4096u, 4096u);              // read one sector content
        for (i = 0u; i < secremain; i++)                            // check if blank sector
        {
            if (W25QXX_BUF[secoff + i] != (uint8_t)0XFF)
            {
                break;
            }
        }
        if (i < secremain)
        {
            W25QXX_Erase_Sector(secpos);                           // not blank, need erase
            for (i = 0u; i < secremain; i++)                        // backup first
            {
                W25QXX_BUF[i + secoff] = pBuffer[i];
            }
            W25QXX_Write_NoCheck(W25QXX_BUF, secpos * 4096u, 4096u); // write back after erase

        }
        else
        {
            W25QXX_Write_NoCheck(pBuffer, WriteAddr, secremain);
        }
        if (NumByteToWrite == secremain)
        {
            break;
        }
        else
        {
            secpos++;                                              // next sector
            secoff          = 0u;

            pBuffer        += secremain;
            WriteAddr      += secremain;
            NumByteToWrite -= secremain;
            if (NumByteToWrite > 4096u)
            {
                secremain = 4096u;
            }
            else
            {
                secremain = NumByteToWrite;
            }
        }
    }
    ;
}

/**
 *******************************************************************************
 ** \brief W25QXX flash chip erase
 **
 ** \param  None
 **
 ** \retval None
 **
 ******************************************************************************/
void W25QXX_Erase_Chip(void)
{
    W25QXX_Write_Enable();
    W25QXX_Wait_Busy();
    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(W25X_ChipErase);
    QSPI_ExitDirectCommMode();
    W25QXX_Wait_Busy();
}

/**
 *******************************************************************************
 ** \brief W25QXX flash sector erase
 **
 ** \param [in] Dst_Addr address
 **
 ** \retval None
 **
 ******************************************************************************/
void W25QXX_Erase_Sector(uint32_t Dst_Addr)
{
    Dst_Addr *= 4096u;
    W25QXX_Write_Enable();
    W25QXX_Wait_Busy();
    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(W25X_SectorErase);
    QSPI_WriteDirectCommValue((uint8_t)((Dst_Addr) >> 16u));
    QSPI_WriteDirectCommValue((uint8_t)((Dst_Addr) >> 8u));
    QSPI_WriteDirectCommValue((uint8_t)Dst_Addr);
    QSPI_ExitDirectCommMode();
    W25QXX_Wait_Busy();
}

/**
 *******************************************************************************
 ** \brief W25QXX flash wait SR.BUSY
 **
 ** \param  None
 **
 ** \retval None
 **
 ******************************************************************************/
void W25QXX_Wait_Busy(void)
{
    while ((W25QXX_ReadSR() & 0x01u) == 0x01u)            // Wait SR.BUSY = 0
    {
        ;
    }
}

/**
 *******************************************************************************
 ** \brief W25QXX enter power down mode
 **
 ** \param  None
 **
 ** \retval None
 **
 ******************************************************************************/
void W25QXX_PowerDown(void)
{
    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(W25X_PowerDown);
    QSPI_ExitDirectCommMode();
    Ddl_Delay1ms(1ul);
}

/**
 *******************************************************************************
 ** \brief W25QXX wake up
 **
 ** \param  None
 **
 ** \retval None
 **
 ******************************************************************************/
void W25QXX_WAKEUP(void)
{
    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(W25X_ReleasePowerDown);
    QSPI_ExitDirectCommMode();
    Ddl_Delay1ms(1ul);
}

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
