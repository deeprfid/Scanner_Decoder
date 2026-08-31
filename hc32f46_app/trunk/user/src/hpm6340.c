
/******************************************************************************/
/** \file main.c
 **
 ** \brief The example of SPI four wire polling tx and rx function
 **
 **   - 2018-11-06  1.0  Yangjp First version for Device Driver Library of SPI.
 **
 ******************************************************************************/

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "hc32_ddl.h"
#include "hc32f46_driver.h"
#include "ipc.h"

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
/* LED0 Port/Pin definition */


/* SPI_SCK Port/Pin definition */
#define SPI_SCK_PORT                    (PortB)
#define SPI_SCK_PIN                     (Pin13)
#define SPI_SCK_FUNC                    (Func_Spi3_Sck)

/* SPI_MOSI Port/Pin definition */
#define SPI_MOSI_PORT                   (PortB)
#define SPI_MOSI_PIN                    (Pin15)
#define SPI_MOSI_FUNC                   (Func_Spi3_Mosi)

/* SPI_MISO Port/Pin definition */
#define SPI_MISO_PORT                   (PortB)
#define SPI_MISO_PIN                    (Pin14)
#define SPI_MISO_FUNC                   (Func_Spi3_Miso)

/* SPI_NSS Port/Pin definition */
#define SPI_NSS_PORT                    (PortB)
#define SPI_NSS_PIN                     (Pin12)
#define SPI_NSS_FUNC                    (Func_Spi3_Nss0)

/* SPI unit and clock definition */
#define SPI_UNIT                        (M4_SPI3)
#define SPI_UNIT_CLOCK                  (PWC_FCG1_PERIPH_SPI3)

/* Choose SPI master or slave mode */
#define SPI_MASTER_MODE
//#define SPI_SLAVE_MODE


#define SPI_DMA_UNIT                    (M4_DMA2)
#define SPI_DMA_CLOCK_UNIT              (PWC_FCG0_PERIPH_DMA2)
#define SPI_DMA_TX_CHANNEL              (DmaCh2)
#define SPI_DMA_RX_CHANNEL              (DmaCh3)
#define SPI_DMA_TX_TRIG_SOURCE          (EVT_SPI3_SPTI)
#define SPI_DMA_RX_TRIG_SOURCE          (EVT_SPI3_SPRI)

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/



/**
 *******************************************************************************
 ** \brief Configure SPI peripheral function
 **
 ** \param [in] None
 **
 ** \retval None
 **
 ******************************************************************************/
//static void Spi_DmaConfig(void)
//{
//    stc_dma_config_t stcDmaCfg;

// 
//    MEM_ZERO_STRUCT(stcDmaCfg);

//    PWC_Fcg0PeriphClockCmd(SPI_DMA_CLOCK_UNIT, Enable);
//    PWC_Fcg0PeriphClockCmd(PWC_FCG0_PERIPH_AOS, Enable);

//   
//    stcDmaCfg.u16BlockSize = 1u;
//    stcDmaCfg.u16TransferCnt = 512;
//    stcDmaCfg.u32SrcAddr = (uint32_t)(&hpmout);
//    stcDmaCfg.u32DesAddr = (uint32_t)(&SPI_UNIT->DR);
//    stcDmaCfg.stcDmaChCfg.enSrcInc = AddressIncrease;
//    stcDmaCfg.stcDmaChCfg.enDesInc = AddressFix;
//    stcDmaCfg.stcDmaChCfg.enTrnWidth = Dma8Bit;
//    stcDmaCfg.stcDmaChCfg.enIntEn = Disable;
//    DMA_InitChannel(SPI_DMA_UNIT, SPI_DMA_TX_CHANNEL, &stcDmaCfg);

//   
//    stcDmaCfg.u16BlockSize = 1u;
//    stcDmaCfg.u16TransferCnt = 512;
//    stcDmaCfg.u32SrcAddr = (uint32_t)(&SPI_UNIT->DR);
//    stcDmaCfg.u32DesAddr = (uint32_t)(&hpmin);
//    stcDmaCfg.stcDmaChCfg.enSrcInc = AddressFix;
//    stcDmaCfg.stcDmaChCfg.enDesInc = AddressIncrease;
//    stcDmaCfg.stcDmaChCfg.enTrnWidth = Dma8Bit;
//    stcDmaCfg.stcDmaChCfg.enIntEn = Disable;
//    DMA_InitChannel(SPI_DMA_UNIT, SPI_DMA_RX_CHANNEL, &stcDmaCfg);

//    DMA_SetTriggerSrc(SPI_DMA_UNIT, SPI_DMA_TX_CHANNEL, SPI_DMA_TX_TRIG_SOURCE);
//    DMA_SetTriggerSrc(SPI_DMA_UNIT, SPI_DMA_RX_CHANNEL, SPI_DMA_RX_TRIG_SOURCE);

//    
//    DMA_Cmd(SPI_DMA_UNIT, Enable);
//}

static void Spi_Config(void)
{
    stc_spi_init_t stcSpiInit;

    /* configuration structure initialization */
    SPI_DeInit(SPI_UNIT);
   // SPI_Cmd(M4_SPI3, Disable);
    //SPI_Cmd(M4_SPI4, Disable);
    MEM_ZERO_STRUCT(stcSpiInit);

    /* Configuration peripheral clock */
    PWC_Fcg1PeriphClockCmd(SPI_UNIT_CLOCK, Enable);

    /* Configuration SPI pin */
    PORT_SetFunc(SPI_SCK_PORT , SPI_SCK_PIN , SPI_SCK_FUNC , Disable);
    PORT_SetFunc(SPI_NSS_PORT , SPI_NSS_PIN , SPI_NSS_FUNC , Disable);
    PORT_SetFunc(SPI_MOSI_PORT, SPI_MOSI_PIN, SPI_MOSI_FUNC, Disable);
    PORT_SetFunc(SPI_MISO_PORT, SPI_MISO_PIN, SPI_MISO_FUNC, Disable);

    /* Configuration SPI structure */
    stcSpiInit.enClkDiv                 = SpiClkDiv16;
    stcSpiInit.enFrameNumber            = SpiFrameNumber1;
    stcSpiInit.enDataLength             = SpiDataLengthBit8;
    stcSpiInit.enFirstBitPosition       = SpiFirstBitPositionMSB;
    stcSpiInit.enSckPolarity            = SpiSckIdleLevelHigh;
    stcSpiInit.enSckPhase               = SpiSckOddSampleEvenChange;
    stcSpiInit.enReadBufferObject       = SpiReadReceiverBuffer;
    stcSpiInit.enWorkMode               = SpiWorkMode4Line;
    stcSpiInit.enTransMode              = SpiTransFullDuplex;
    stcSpiInit.enCommAutoSuspendEn      = Disable;
    stcSpiInit.enModeFaultErrorDetectEn = Disable;
    stcSpiInit.enParitySelfDetectEn     = Disable;
    stcSpiInit.enParityEn               = Disable;

    stcSpiInit.enMasterSlaveMode         = SpiModeMaster;
    stcSpiInit.stcSsConfig.enSsValidBit  = SpiSsValidChannel0;
    stcSpiInit.stcSsConfig.enSs0Polarity = SpiSsLowValid;
    
    stcSpiInit.stcDelayConfig.enSsSetupDelayOption = SpiSsSetupDelayCustomValue;
    stcSpiInit.stcDelayConfig.enSsSetupDelayTime = SpiSsSetupDelaySck1;
    stcSpiInit.stcDelayConfig.enSsHoldDelayOption = SpiSsHoldDelayCustomValue;
    stcSpiInit.stcDelayConfig.enSsHoldDelayTime = SpiSsHoldDelaySck1;
    stcSpiInit.stcDelayConfig.enSsIntervalTimeOption = SpiSsIntervalCustomValue;
    stcSpiInit.stcDelayConfig.enSsIntervalTime = SpiSsIntervalSck6PlusPck2;
    SPI_Init(SPI_UNIT, &stcSpiInit);
   // Spi_DmaConfig();
   SPI_Cmd(SPI_UNIT, Enable);
}

/**
 *******************************************************************************
 ** \brief Configure SPI DMA function
 **
 ** \param [in] None
 **
 ** \retval None
 **
 ******************************************************************************/



void DMA_Reload(uint8_t *sendbuff,uint8_t *revbuff,uint16_t len)
{

       // Spi_Config();
        DMA_SetSrcAddress(SPI_DMA_UNIT, SPI_DMA_TX_CHANNEL, (uint32_t)(&sendbuff[0]));
        DMA_SetTransferCnt(SPI_DMA_UNIT, SPI_DMA_TX_CHANNEL, len);
        DMA_SetDesAddress(SPI_DMA_UNIT, SPI_DMA_RX_CHANNEL, (uint32_t)(&revbuff[0]));
        DMA_SetTransferCnt(SPI_DMA_UNIT, SPI_DMA_RX_CHANNEL, len);
        /* Enable DMA channel */
        //DMA_Cmd(M4_DMA2,Enable);
        DMA_ChannelCmd(SPI_DMA_UNIT, SPI_DMA_TX_CHANNEL, Enable);
        DMA_ChannelCmd(SPI_DMA_UNIT, SPI_DMA_RX_CHANNEL, Enable);
        /* Enable SPI to start DMA */
        SPI_Cmd(SPI_UNIT, Enable);
        while (Reset == DMA_GetIrqFlag(SPI_DMA_UNIT, SPI_DMA_TX_CHANNEL, TrnCpltIrq))
        {
        }
       while (Reset == DMA_GetIrqFlag(SPI_DMA_UNIT, SPI_DMA_RX_CHANNEL, TrnCpltIrq))
       {
       }
        DMA_ClearIrqFlag(SPI_DMA_UNIT, SPI_DMA_TX_CHANNEL, TrnCpltIrq);
        DMA_ClearIrqFlag(SPI_DMA_UNIT, SPI_DMA_RX_CHANNEL, TrnCpltIrq);
        SPI_Cmd(SPI_UNIT, Disable);
        sleep_ms(5);

}


/**
 *******************************************************************************
 ** \brief  main function for four wire SPI polling tx and rx function
 **
 ** \param [in]  None
 **
 ** \retval int32_t Return value, if needed
 **
 ******************************************************************************/
void hpm6K_SPI_reset(uint32_t rstcnt)
{
  for(uint32_t i=0;i<rstcnt;i++)
   { 
    PORT_SetBits(PortC,Pin14);
    PORT_ResetBits(PortC,Pin14);
    lkt_delayus(10); 
    PORT_SetBits(PortC,Pin14);

   }       
}
uint8_t SPI_HPM6340_Init(void)
{

    /* Configure SPI */

    hpm6K_SPI_reset(1);
    Spi_Config();
    sleep_ms(1);
    //lkt_delayus(150);


    /* Get tx buffer length */
    return 1; 
}

uint8_t HPM_write_read_byte(uint8_t byte)
{
    while (Reset == SPI_GetFlag(SPI_UNIT, SpiFlagSendBufferEmpty)) {}
    SPI_SendData8(SPI_UNIT, byte);
    while (Reset == SPI_GetFlag(SPI_UNIT, SpiFlagReceiveBufferFull)) {}
    return (SPI_ReceiveData8(SPI_UNIT)&0xff);
}

uint8_t HPM6340_spi_readbyte(void)
{
		return HPM_write_read_byte(0x00);
}
void   HPM6340_spi_writebyte(uint8_t wb)
{
		HPM_write_read_byte(wb);
}
void SPI_page_wr(uint8_t *wbuf,uint8_t *rbuf)
{
  
  if(rbuf!=NULL)
    {
      //DMA_Reload(wbuf,(uint8_t*)&hpmin,SPI_SOC_TRANSFER_COUNT_MAX);
    for (uint16_t i=0; i< SPI_SOC_TRANSFER_COUNT_MAX;i++)
       {
          rbuf[i]=HPM_write_read_byte(wbuf[i]);
       }
    }
 else
    {
     for (uint16_t i=0; i< SPI_SOC_TRANSFER_COUNT_MAX;i++)
       {
          HPM_write_read_byte(wbuf[i]);
       }
    // DMA_Reload(wbuf,(uint8_t*)&hpmin,SPI_SOC_TRANSFER_COUNT_MAX);
    }
}

uint8_t SPI_read_Write_data(uint8_t *wbuf,uint8_t *rbuf, uint16_t len)
{
 uint16_t index = 0u;
 uint8_t  spitransfercnt=0;
          SPI_HPM6340_Init(); 
          if((len % SPI_SOC_TRANSFER_COUNT_MAX)==0)
           {
            spitransfercnt=(len/SPI_SOC_TRANSFER_COUNT_MAX);
            index=0;
            
           }
           else
           {
            spitransfercnt=(len/SPI_SOC_TRANSFER_COUNT_MAX)+1;
            index=1;
           }
      
        
        if(rbuf!=NULL)
         {
         for (uint16_t i=0; i<spitransfercnt;i++)
          {
              
              SPI_page_wr(wbuf+i*SPI_SOC_TRANSFER_COUNT_MAX,rbuf);
             // sleep_ms(1);

              // SPI_HPM6340_Init(); 
              //lkt_delayus(10);
             
          }
         }
         else
         {
          for (uint16_t i=0; i<spitransfercnt;i++)
          {
             // SPI_HPM6340_Init();
              SPI_page_wr(wbuf+i*SPI_SOC_TRANSFER_COUNT_MAX,rbuf);
              sleep_ms(1);

             // SPI_HPM6340_Init();
              //lkt_delayus(10);
          }
         }
       /* if(rbuf==NULL)
        {

         while (index < len)
        {
            
            while (Reset == SPI_GetFlag(SPI_UNIT, SpiFlagSendBufferEmpty))
            {
            }
          
            SPI_SendData8(SPI_UNIT, wbuf[index]);
            while (Reset == SPI_GetFlag(SPI_UNIT, SpiFlagReceiveBufferFull))
            {
            }
           
           rdnull = SPI_ReceiveData8(SPI_UNIT);
           
            index++;
        }

        }
       else
       {  
        while (index < len)
        {
           
            while (Reset == SPI_GetFlag(SPI_UNIT, SpiFlagSendBufferEmpty))
            {
            }
            
            SPI_SendData8(SPI_UNIT, wbuf[index]);
            
            while (Reset == SPI_GetFlag(SPI_UNIT, SpiFlagReceiveBufferFull))
            {
            }
           
            rbuf[index] = SPI_ReceiveData8(SPI_UNIT);
            index++;
        }
       }*/
   // LICENSE_Spi_Config(); 
  return index;

}

uint8_t hpm_op(uint8_t *wtbuf,uint8_t *rdbuf,uint16_t buflen)
{
  uint8_t index = 0u;
   index= SPI_read_Write_data(wtbuf,rdbuf,buflen);
  return index;    
}

uint8_t hpm_op_read(uint8_t *rdbuf,uint16_t buflen)
{
    uint16_t i=0;
    SPI_HPM6340_Init(); 
    for ( i=0; i< SPI_SOC_TRANSFER_COUNT_MAX;i++)
    {
      rdbuf[i]= HPM6340_spi_readbyte();
    
    }
   
  return i;    
}

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
