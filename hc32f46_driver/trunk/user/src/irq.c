#include "irq.h"
#include "timer.h"
#include "uart.h"
#include "hc32f46_driver.h"
//ÖÐ¶Ï·ÖÅä

//Int001_IRQn  uart1 rec
//Int002_IRQn  uart1 err

//Int003_IRQn  uart2 rec
//Int004_IRQn  uart2 err

//Int005_IRQn  uart3 rec
//Int006_IRQn  uart3 err

//Int007_IRQn  uart4 rec
//Int008_IRQn  uart4 err

//Int009_IRQn  time4


void uart_err_clear(int s);

void Usart1recErrIrqCallback_dma(void) //12us
{
	uart_err_clear(0);
	uart_reloaddmarxptr(0);
}

void Usart2recErrIrqCallback_dma(void) //12us
{
	uart_err_clear(1);
	uart_reloaddmarxptr(1);
}

void Usart2recErrIrqCallback(void)//1.5us
{
    if (Set == USART_GetStatus(M4_USART2, UsartFrameErr))
    {
        USART_ClearStatus(M4_USART2, UsartFrameErr);
    }

    if (Set == USART_GetStatus(M4_USART2, UsartParityErr))
    {
        USART_ClearStatus(M4_USART2, UsartParityErr);
    }
    if (Set == USART_GetStatus(M4_USART2, UsartOverrunErr))
    {
        USART_ClearStatus(M4_USART2, UsartOverrunErr);
    }
}

void Usart3recErrIrqCallback_dma(void) //12us
{
	  uart_err_clear(2);
    uart_reloaddmarxptr(2);
}

void Usart3recErrIrqCallback(void)//1.5us
{
    if (Set == USART_GetStatus(M4_USART3, UsartFrameErr))
    {
        USART_ClearStatus(M4_USART3, UsartFrameErr);
    }

    if (Set == USART_GetStatus(M4_USART3, UsartParityErr))
    {
        USART_ClearStatus(M4_USART3, UsartParityErr);
    }
    if (Set == USART_GetStatus(M4_USART3, UsartOverrunErr))
    {
        USART_ClearStatus(M4_USART3, UsartOverrunErr);
    }
}

void Usart4recErrIrqCallback_dma(void) //12us
{
	  uart_err_clear(3);
    uart_reloaddmarxptr(2);
}

void Usart4recErrIrqCallback(void)//1.5us
{
    if (Set == USART_GetStatus(M4_USART4, UsartFrameErr))
    {
        USART_ClearStatus(M4_USART4, UsartFrameErr);
    }
    if (Set == USART_GetStatus(M4_USART4, UsartParityErr))
    {
        USART_ClearStatus(M4_USART4, UsartParityErr);
    }
    if (Set == USART_GetStatus(M4_USART4, UsartOverrunErr))
    {
        USART_ClearStatus(M4_USART4, UsartOverrunErr);
    }
}

extern stc_dma_config_t stcDma2ch0Init;
extern stc_dma_config_t stcDma1ch0Init;

volatile uint32 gSysTickCnt = 0;
volatile int gIsRunDhcpTimeHandler = 0;
void DHCP_time_handler(void);
volatile int gIsRunDnsTimeHandler = 0;
void DNS_time_handler(void);
//int testcnt = 0;
volatile uint32 gResetKeyDownCnt = 0,gFuncKeyDownCnt=0;
BtnResetCallback gBtnResetCb = NULL;
uint8_t  Keyvalue=0;
void TimeraUnit4_IrqCallback(void)
{
    gSysTickCnt++;
    SWDT_RefreshCounter();
	 if (gIsRunDhcpTimeHandler == 1)
	 {
		 if (gSysTickCnt % 2 == 0)
		 DHCP_time_handler();
	 }
	 if (gIsRunDnsTimeHandler == 1)
	 {
		 if (gSysTickCnt % 2 == 0)
		 DNS_time_handler();
	 }
	 if (get_ipreset_key_value() == 0)
	 {
		 gResetKeyDownCnt++;
		 if (gResetKeyDownCnt == 10)
		 {
			 BtParams_ST btParams;
			 getBtParams(&btParams);
			 if (btParams.updateflag != 0 && (btParams.updatemode == FwUpdateMode_ByFtp_FmEth || 
				 btParams.updatemode == FwUpdateMode_ByFtp_Fm4G))
			 {
				 btParams.updatemode = FwUpdateMode_Default;
				 setBtParams(&btParams);
				 sleep_ms(50);
			 }
			 set_default_network_config();
			 erase_multi_config(ERASE_FLS_CFG_BIT_ACTMODE | ERASE_FLS_CFG_BIT_WKMODEPARA | ERASE_FLS_CFG_BIT_PSVMODE);
			 Erase_eastag_to_flash();
			 sleep_ms(50);
			 if (get_uart_ex_dev() == Uart_Ex_Wlan)
			 {
				 erase_wlan_config();
				 sleep_ms(50);
			 }
			 
			 if (get_uart_ex_dev() == Uart_Ex_Bluetooth)
			 {
				 erase_bluetooth_config();
				 sleep_ms(50);
			 }
						 
			 if (gBtnResetCb != NULL)
				 gBtnResetCb();
			   system_reset();
		 }
	 }
	 else
	 {
		 gResetKeyDownCnt = 0;
	 }
	 if(gpi_get(1)==0) // FuncKey scanning
	 {
	     if(gFuncKeyDownCnt++==1)
			 {
			    Keyvalue=1;
			 }	 
	 
	 }
   else
   {
		Keyvalue=0; 
    gFuncKeyDownCnt=0;
	 }		 
	 
}



void Usart2RxIrqCallback(void) //1us
{
    if (Set == USART_GetStatus(M4_USART2, UsartRxNoEmpty ))
    {
        gUart1RecvBuf[uart2reccount] = (0xff&USART_RecData(M4_USART2));
        uart2reccount++;
        if(uart2reccount >= MAX_UART1_BUF_SIZE)
			  uart2reccount = 0;
    }
}

void Usart3RxIrqCallback(void) //1us
{
    if (Set == USART_GetStatus(M4_USART3, UsartRxNoEmpty ))
    {
        gUart2RecvBuf[uart3reccount] = (0xff&USART_RecData(M4_USART3));
        uart3reccount++;
        if(uart3reccount >= MAX_UART2_BUF_SIZE) 
			  uart3reccount = 0;
    }
}

void Usart4RxIrqCallback(void) //1us
{
    if (Set == USART_GetStatus(M4_USART4, UsartRxNoEmpty ))
    {
        gUart3RecvBuf[uart4reccount]=(0xff&USART_RecData(M4_USART4));
        uart4reccount++;
        if(uart4reccount>=MAX_UART3_BUF_SIZE) uart4reccount=0;
    }
}


void Dma1ch0tcIrqCallback(void)//3us
{
    M4_DMA1->INTCLR1 |= (1ul << (DmaCh0 + 0));
    DMA_InitChannel_fast(M4_DMA1, DmaCh0, &stcDma1ch0Init);
    DMA_ChannelCmd(M4_DMA1, DmaCh0, Enable);
}



void Dma2ch0tcIrqCallback(void)//3us
{
//    uart1_DmaRX_Init_fast();
    M4_DMA2->INTCLR1 |= (1ul << (DmaCh0 + 0));
    DMA_InitChannel_fast(M4_DMA2, DmaCh0, &stcDma2ch0Init);
    DMA_ChannelCmd(M4_DMA2, DmaCh0, Enable);
}



