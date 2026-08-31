#include "uart.h"
#include "irq.h"
#include "pio.h"
#include <stdlib.h>


commonUartParaLocal gUartParams[TOTAL_UART_NUM];


uint8 gUart0RecvBuf[MAX_UART0_BUF_SIZE];
uint8 *gUart1RecvBuf = NULL;
uint8 gUart2RecvBuf[MAX_UART2_BUF_SIZE];
uint8 *gUart3RecvBuf = NULL;


/*串口DMA分配
uart1   RFID通信    DMA2  ch0
uart3   串口        DMA1  ch0
*/

//RX_DMA_BUFFER  rxbuffers[2]; //2块DMA 存储区 512Byte X2
//PRX_DMA_BUFFER uart1_prx_front= &rxbuffers[0];
//PRX_DMA_BUFFER uart3_prx_front= &rxbuffers[1];


//uart2 uart4中断接收

volatile uint16_t  uart3reccount = 0;
volatile uint16_t  uart2reccount = 0;
volatile uint16_t  uart4reccount = 0;


stc_dma_config_t stcDma2ch0Init;
stc_dma_config_t stcDma1ch0Init;



int hc32f460_uart_init(uart_cfg_para_st *ucpst)
{
    if(ucpst->uartid == 0)       
	 {
		 gUartParams[ucpst->uartid].recvbuf = gUart0RecvBuf;
		 gUartParams[ucpst->uartid].recvbufsize = MAX_UART0_BUF_SIZE;
		 ucpst->recvbuf = gUart0RecvBuf;
		 ucpst->buflen = MAX_UART0_BUF_SIZE;
		 uart1_Init(ucpst);
	 }
    else if(ucpst->uartid == 1) 
	 {
		 if (gUart1RecvBuf == NULL)
		 {
	//		 gUart1RecvBuf = malloc_hexp(MAX_UART1_BUF_SIZE);
			 int tmpaddr;
			 gUart1RecvBuf = malloc_hexp(MAX_UART1_BUF_SIZE+8);
			 tmpaddr = (int)gUart1RecvBuf;
			 if (tmpaddr % 8 != 0)
			 {
				 tmpaddr += 8 - (tmpaddr % 8);
				 gUart1RecvBuf = (uint8 *)tmpaddr;
			 }
		 }

		 gUartParams[ucpst->uartid].recvbuf = gUart1RecvBuf;
		 gUartParams[ucpst->uartid].recvbufsize = MAX_UART1_BUF_SIZE;
		 uart2reccount = 0;
		 uart2_Init(ucpst);
	 }
	 else if(ucpst->uartid == 2)
	 {
		 gUartParams[ucpst->uartid].recvbuf = gUart2RecvBuf;
		 gUartParams[ucpst->uartid].recvbufsize = MAX_UART2_BUF_SIZE;
		 ucpst->recvbuf = gUart2RecvBuf;
		 ucpst->buflen = MAX_UART2_BUF_SIZE;
		 uart3reccount = 0;
		 uart3_Init(ucpst);
	 }
    else if(ucpst->uartid == 3)
	 {
		 if (gUart3RecvBuf == NULL)
			 gUart3RecvBuf = malloc_hexp(MAX_UART3_BUF_SIZE);
		 
		 gUartParams[ucpst->uartid].recvbuf = gUart3RecvBuf;
		 gUartParams[ucpst->uartid].recvbufsize = MAX_UART3_BUF_SIZE;
		 uart4reccount = 0;
		 uart4_Init(ucpst);
	 }
	 else
	 {
		 TRACE("hc32f460_uart_init invlaid uartid\n");
		 return -1;		 
	 }
	 
    return 0;
}

int uart_DmaRX_Init(int dma_id, M4_USART_TypeDef *M4_USART_ID, 
	en_event_src_t EVT_USART_ID_RI, IRQn_Type Int_ID_IRQn, 
	int DDL_IRQ_PRIORITY_ID, uint16_t cont,uint8_t *recadd) //DMA2 ch0 
{
    stc_dma_config_t stcDmaInit;
    stc_irq_regi_conf_t stcIrqRegiCfg;
	
    /* Initialize DMA. */
    MEM_ZERO_STRUCT(stcDmaInit);
    stcDmaInit.u16BlockSize = 1u; /* 1 block */
    stcDmaInit.u16TransferCnt=cont;
    stcDmaInit.u32SrcAddr = ((uint32_t)(&M4_USART_ID->DR)+2ul); /* Set source address. */
    stcDmaInit.u32DesAddr=(uint32_t)recadd;
    stcDmaInit.stcDmaChCfg.enSrcInc = AddressFix;  /* Set source address mode. */
    stcDmaInit.stcDmaChCfg.enDesInc = AddressIncrease;  /* Set destination address mode. */
    stcDmaInit.stcDmaChCfg.enIntEn =Enable;       /* Disable interrupt. */
    stcDmaInit.stcDmaChCfg.enTrnWidth = Dma8Bit;   /* Set data width 8bit. */
	
	/////////////
	if (dma_id == 2)
	{
		 MEM_ZERO_STRUCT(stcDma2ch0Init);
		 stcDma2ch0Init.u16BlockSize = 1u; /* 1 block */
		 stcDma2ch0Init.u16TransferCnt=cont;
		 stcDma2ch0Init.u32SrcAddr = ((uint32_t)(&M4_USART_ID->DR)+2ul); /* Set source address. */
		 stcDma2ch0Init.u32DesAddr=(uint32_t)recadd;
		 stcDma2ch0Init.stcDmaChCfg.enSrcInc = AddressFix;  /* Set source address mode. */
		 stcDma2ch0Init.stcDmaChCfg.enDesInc = AddressIncrease;  /* Set destination address mode. */
		 stcDma2ch0Init.stcDmaChCfg.enIntEn =Enable;       /* Disable interrupt. */
		 stcDma2ch0Init.stcDmaChCfg.enTrnWidth = Dma8Bit;   /* Set data width 8bit. */
		
		 /* Enable DMA. */
		 DMA_Cmd(M4_DMA2,Enable);
		 DMA_InitChannel(M4_DMA2, DmaCh0, &stcDmaInit);
		 DMA_ChannelCmd(M4_DMA2, DmaCh0, Enable);
	}
	else if (dma_id == 1)
	{
		 MEM_ZERO_STRUCT(stcDma1ch0Init);
		 stcDma1ch0Init.u16BlockSize = 1u; /* 1 block */
		 stcDma1ch0Init.u16TransferCnt=cont;
		 stcDma1ch0Init.u32SrcAddr = ((uint32_t)(&M4_USART_ID->DR)+2ul); /* Set source address. */
		 stcDma1ch0Init.u32DesAddr=(uint32_t)recadd;
		 stcDma1ch0Init.stcDmaChCfg.enSrcInc = AddressFix;  /* Set source address mode. */
		 stcDma1ch0Init.stcDmaChCfg.enDesInc = AddressIncrease;  /* Set destination address mode. */
		 stcDma1ch0Init.stcDmaChCfg.enIntEn =Enable;       /* Disable interrupt. */
		 stcDma1ch0Init.stcDmaChCfg.enTrnWidth = Dma8Bit;   /* Set data width 8bit. */	
		
		 /* Enable DMA. */
		 DMA_Cmd(M4_DMA1,Enable);
		 DMA_InitChannel(M4_DMA1, DmaCh0, &stcDmaInit);
		 DMA_ChannelCmd(M4_DMA1, DmaCh0, Enable);
	}
	else
		return -1;
	////////////


    /* Enable PTDIS(AOS) clock*/
	 stcIrqRegiCfg.enIRQn = Int_ID_IRQn;
	
    PWC_Fcg0PeriphClockCmd(PWC_FCG0_PERIPH_AOS,Enable);
	 if (dma_id == 2)
	 {
		stcIrqRegiCfg.pfnCallback = &Dma2ch0tcIrqCallback;
        stcIrqRegiCfg.enIntSrc = INT_DMA2_TC0;
		DMA_SetTriggerSrc(M4_DMA2, DmaCh0, EVT_USART_ID_RI);
	 }
	 else
	 {
		 stcIrqRegiCfg.pfnCallback = &Dma1ch0tcIrqCallback;
         stcIrqRegiCfg.enIntSrc = INT_DMA1_TC0;
		 DMA_SetTriggerSrc(M4_DMA1, DmaCh0, EVT_USART_ID_RI);
	 }
	 
    enIrqRegistration(&stcIrqRegiCfg);
    NVIC_SetPriority(stcIrqRegiCfg.enIRQn, DDL_IRQ_PRIORITY_ID);
    NVIC_ClearPendingIRQ(stcIrqRegiCfg.enIRQn);
    NVIC_EnableIRQ(stcIrqRegiCfg.enIRQn);
	 return 0;
}


int uart1_Init(uart_cfg_para_st *ucpst)//串口初始化配置函数，即是上电后的配置函数,配置成功返回0，不成功返回1
{
    stc_irq_regi_conf_t stcIrqRegiCfg;
  //  uint8_t ch;
    USART_FuncCmd(M4_USART1, UsartRx, Disable);
    USART_FuncCmd(M4_USART1, UsartTx, Disable);
    USART_FuncCmd(M4_USART1, UsartRxInt, Disable);

    const stc_usart_uart_init_t stcInitCfg =
    {
        UsartIntClkCkNoOutput, //时钟不输出
        (ucpst->baud < 115200)?UsartClkDiv_64:UsartClkDiv_1, //波特率分频
        (en_usart_data_len_t)ucpst->databits,//8bits
        UsartDataLsbFirst,//LSB优先
        (en_usart_stop_bit_t)ucpst->stopbits,//1bit 停止位
        (en_usart_parity_t)ucpst->parity,//无奇偶校验
        UsartSampleBit8, //8bit采样周期
        UsartStartBitFallEdge,//下降沿开始
        (en_usart_hw_flow_ctrl_t)ucpst->flowctol,      //rts en
    };

    /* Initialize USART IO  Disable 禁止副功能 */
    PORT_SetFunc(PortA,Pin03, Func_Usart1_Rx, Disable);
    PORT_SetFunc(PortA,Pin02, Func_Usart1_Tx, Disable);

    /* Initialize UART */
    USART_UART_Init(M4_USART1, &stcInitCfg);
    /* Set baudrate */
    USART_SetBaudrate(M4_USART1, ucpst->baud);

    //DMA初始化
    (0xff&USART_RecData(M4_USART1));//DMA溢出后要做这一步恢复
    //uart1_DmaTX_Init();
	 uart_DmaRX_Init(2, M4_USART1, EVT_USART1_RI, Int001_IRQn, 
		DDL_IRQ_PRIORITY_00, MAX_UART0_BUF_SIZE, gUart0RecvBuf);

    USART_ClearStatus(M4_USART1, UsartFrameErr);
    USART_ClearStatus(M4_USART1, UsartParityErr);
    USART_ClearStatus(M4_USART1, UsartOverrunErr);

    /* Set USART RX error IRQ */
    stcIrqRegiCfg.enIRQn = Int002_IRQn;
    stcIrqRegiCfg.pfnCallback = &Usart1recErrIrqCallback_dma;
    stcIrqRegiCfg.enIntSrc = INT_USART1_EI;
    enIrqRegistration(&stcIrqRegiCfg);
    NVIC_SetPriority(stcIrqRegiCfg.enIRQn, DDL_IRQ_PRIORITY_04);
    NVIC_ClearPendingIRQ(stcIrqRegiCfg.enIRQn);
    NVIC_EnableIRQ(stcIrqRegiCfg.enIRQn);

    /*Enable RX && RX interupt function*/
    USART_FuncCmd(M4_USART1, UsartRx, Enable);
    USART_FuncCmd(M4_USART1, UsartTx, Enable);
    USART_FuncCmd(M4_USART1, UsartRxInt, Enable);
//    uart1_prx_front->length=0;
    return  0;
}

int uart2_Init(uart_cfg_para_st *ucpst)//串口初始化配置函数，即是上电后的配置函数,配置成功返回0，不成功返回1
{
//    uint8_t ch;
    stc_irq_regi_conf_t stcIrqRegiCfg;
    USART_FuncCmd(M4_USART2, UsartRx, Disable);
    USART_FuncCmd(M4_USART2, UsartTx, Disable);
    USART_FuncCmd(M4_USART2, UsartRxInt, Disable);

    const stc_usart_uart_init_t stcInitCfg =
    {
        UsartIntClkCkNoOutput, //时钟不输出
        (ucpst->baud<115200)?UsartClkDiv_64:UsartClkDiv_1, //波特率分频
        (en_usart_data_len_t)ucpst->databits,//8bits
        UsartDataLsbFirst,//LSB优先
        (en_usart_stop_bit_t)ucpst->stopbits,//1bit 停止位
        (en_usart_parity_t)ucpst->parity,//无奇偶校验
        UsartSampleBit8, //8bit采样周期
        UsartStartBitFallEdge,//下降沿开始
        (en_usart_hw_flow_ctrl_t)ucpst->flowctol,      //rts en
    };


    /* Initialize USART IO  Disable 禁止副功能 */
    PORT_SetFunc(PortA,Pin01, Func_Usart2_Rx, Disable);
    PORT_SetFunc(PortA,Pin00, Func_Usart2_Tx, Disable);

    /* Initialize UART */
    USART_UART_Init(M4_USART2, &stcInitCfg);
    /* Set baudrate */
    USART_SetBaudrate(M4_USART2, ucpst->baud);

    //DMA初始化
    (0xff&USART_RecData(M4_USART2));//DMA溢出后要做这一步恢复
	 if (ucpst->isrdma == 1)
	 {
		 uart_DmaRX_Init(1, M4_USART2, EVT_USART2_RI, Int005_IRQn, 
			DDL_IRQ_PRIORITY_01, MAX_UART1_BUF_SIZE, gUart1RecvBuf);
//		 printf("uart2_Init if (ucpst->isrdma == 1) 11111111111\n");
	 }

    USART_ClearStatus(M4_USART2, UsartFrameErr);
    USART_ClearStatus(M4_USART2, UsartParityErr);
    USART_ClearStatus(M4_USART2, UsartOverrunErr);

    /* Set USART RX IRQ */
	 if (ucpst->isrdma == 0)
	 {
		 stcIrqRegiCfg.enIRQn = Int003_IRQn;
		 stcIrqRegiCfg.pfnCallback = &Usart2RxIrqCallback;
		 stcIrqRegiCfg.enIntSrc = INT_USART2_RI;
		 enIrqRegistration(&stcIrqRegiCfg);
		 NVIC_SetPriority(stcIrqRegiCfg.enIRQn, DDL_IRQ_PRIORITY_02);
		 NVIC_ClearPendingIRQ(stcIrqRegiCfg.enIRQn);
		 NVIC_EnableIRQ(stcIrqRegiCfg.enIRQn);
	 }
    /* Set USART RX error IRQ */
    stcIrqRegiCfg.enIRQn = Int004_IRQn;
	 if (ucpst->isrdma == 0)
		stcIrqRegiCfg.pfnCallback = &Usart2recErrIrqCallback;
	 else
		 stcIrqRegiCfg.pfnCallback = &Usart2recErrIrqCallback_dma;
	 
    stcIrqRegiCfg.enIntSrc = INT_USART2_EI;
    enIrqRegistration(&stcIrqRegiCfg);
    NVIC_SetPriority(stcIrqRegiCfg.enIRQn, DDL_IRQ_PRIORITY_05);
    NVIC_ClearPendingIRQ(stcIrqRegiCfg.enIRQn);
    NVIC_EnableIRQ(stcIrqRegiCfg.enIRQn);

    /*Enable RX && RX interupt function*/
    USART_FuncCmd(M4_USART2, UsartRx, Enable);
    USART_FuncCmd(M4_USART2, UsartTx, Enable);
    USART_FuncCmd(M4_USART2, UsartRxInt, Enable);
    return  0;
}

int uart3_Init(uart_cfg_para_st *ucpst)//串口初始化配置函数，即是上电后的配置函数,配置成功返回0，不成功返回1
{  // uint8_t ch;
    stc_irq_regi_conf_t stcIrqRegiCfg;
    USART_FuncCmd(M4_USART3, UsartRx, Disable);
    USART_FuncCmd(M4_USART3, UsartTx, Disable);
    USART_FuncCmd(M4_USART3, UsartRxInt, Disable);

    const stc_usart_uart_init_t stcInitCfg =
    {
        UsartIntClkCkNoOutput, //时钟不输出
        (ucpst->baud<115200)?UsartClkDiv_64:UsartClkDiv_1, //波特率分频
        (en_usart_data_len_t)ucpst->databits,//8bits
        UsartDataLsbFirst,//LSB优先
        (en_usart_stop_bit_t)ucpst->stopbits,//1bit 停止位
        (en_usart_parity_t)ucpst->parity,//无奇偶校验
        UsartSampleBit8, //8bit采样周期
        UsartStartBitFallEdge,//下降沿开始
        (en_usart_hw_flow_ctrl_t)ucpst->flowctol,      //rts en
    };

//	 TRACE("3333333333333333333333333333 databits:%d, stopbits:%d, parity:%d, flowctol:%d\n", 
//		ucpst->databits, ucpst->stopbits, ucpst->parity, ucpst->flowctol);
    /* Initialize USART IO  Disable 禁止副功能 */
    PORT_SetFunc(PortB,Pin06, Func_Usart3_Rx, Disable);
    PORT_SetFunc(PortB,Pin07, Func_Usart3_Tx, Disable);

    /* Initialize UART */
    USART_UART_Init(M4_USART3, &stcInitCfg);
    /* Set baudrate */
    USART_SetBaudrate(M4_USART3, ucpst->baud);

    //DMA初始化
    (0xff&USART_RecData(M4_USART3));//DMA溢出后要做这一步恢复
	 if (ucpst->isrdma == 1)
	 {
		 uart_DmaRX_Init(1, M4_USART3, EVT_USART3_RI, Int005_IRQn, 
			DDL_IRQ_PRIORITY_01, MAX_UART2_BUF_SIZE, gUart2RecvBuf);
//		 printf("uart3_Init  if (ucpst->isrdma == 1) 1111111111111\n");
	 }
    USART_ClearStatus(M4_USART3, UsartFrameErr);
    USART_ClearStatus(M4_USART3, UsartParityErr);
    USART_ClearStatus(M4_USART3, UsartOverrunErr);
	 
    /* Set USART RX IRQ */
	 if (ucpst->isrdma == 0)
	 {
		 stcIrqRegiCfg.enIRQn = Int011_IRQn;
		 stcIrqRegiCfg.pfnCallback = &Usart3RxIrqCallback;
		 stcIrqRegiCfg.enIntSrc = INT_USART3_RI;
		 enIrqRegistration(&stcIrqRegiCfg);
		 NVIC_SetPriority(stcIrqRegiCfg.enIRQn, DDL_IRQ_PRIORITY_02);
		 NVIC_ClearPendingIRQ(stcIrqRegiCfg.enIRQn);
		 NVIC_EnableIRQ(stcIrqRegiCfg.enIRQn);
//		 printf("uart3_Init  if (ucpst->isrdma == 0) 000000000000\n");
	 }
	 
    /* Set USART RX error IRQ */
    stcIrqRegiCfg.enIRQn = Int006_IRQn;
	 if (ucpst->isrdma == 0)
		 stcIrqRegiCfg.pfnCallback = &Usart3recErrIrqCallback;
	 else
		 stcIrqRegiCfg.pfnCallback = &Usart3recErrIrqCallback_dma;
    stcIrqRegiCfg.enIntSrc = INT_USART3_EI;
    enIrqRegistration(&stcIrqRegiCfg);
    NVIC_SetPriority(stcIrqRegiCfg.enIRQn, DDL_IRQ_PRIORITY_05);
    NVIC_ClearPendingIRQ(stcIrqRegiCfg.enIRQn);
    NVIC_EnableIRQ(stcIrqRegiCfg.enIRQn);

    /*Enable RX && RX interupt function*/
    USART_FuncCmd(M4_USART3, UsartRx, Enable);
    USART_FuncCmd(M4_USART3, UsartTx, Enable);
    USART_FuncCmd(M4_USART3, UsartRxInt, Enable);
//    uart3_prx_front->length=0;
    return  0;
}


int uart4_Init(uart_cfg_para_st *ucpst)//串口初始化配置函数，即是上电后的配置函数,配置成功返回0，不成功返回1
{
//    uint8_t ch;
    stc_irq_regi_conf_t stcIrqRegiCfg;
    USART_FuncCmd(M4_USART4, UsartRx, Disable);
    USART_FuncCmd(M4_USART4, UsartTx, Disable);
    USART_FuncCmd(M4_USART4, UsartRxInt, Disable);

    const stc_usart_uart_init_t stcInitCfg =
    {
        UsartIntClkCkNoOutput, //时钟不输出
        (ucpst->baud<115200)?UsartClkDiv_64:UsartClkDiv_1, //波特率分频
        (en_usart_data_len_t)ucpst->databits,//8bits
        UsartDataLsbFirst,//LSB优先
        (en_usart_stop_bit_t)ucpst->stopbits,//1bit 停止位
        (en_usart_parity_t)ucpst->parity,//无奇偶校验
        UsartSampleBit8, //8bit采样周期
        UsartStartBitFallEdge,//下降沿开始
        (en_usart_hw_flow_ctrl_t)ucpst->flowctol,      //rts en
    };


    /* Initialize USART IO  Disable 禁止副功能 */
    PORT_SetFunc(PortH,Pin03, Func_Usart4_Rx, Disable);
    PORT_SetFunc(PortC,Pin13, Func_Usart4_Tx, Disable);

    /* Initialize UART */
    USART_UART_Init(M4_USART4, &stcInitCfg);
    /* Set baudrate */
    USART_SetBaudrate(M4_USART4, ucpst->baud);

    //DMA初始化
    (0xff&USART_RecData(M4_USART4));//DMA溢出后要做这一步恢复
    //uart4_DmaTX_Init();
    //init_dma1ch2cfg();
    //uart4_DmaRX_Init(bufflen,addr);

    USART_ClearStatus(M4_USART4, UsartFrameErr);
    USART_ClearStatus(M4_USART4, UsartParityErr);
    USART_ClearStatus(M4_USART4, UsartOverrunErr);

    /* Set USART RX IRQ */
    stcIrqRegiCfg.enIRQn = Int007_IRQn;
    stcIrqRegiCfg.pfnCallback = &Usart4RxIrqCallback;
    stcIrqRegiCfg.enIntSrc = INT_USART4_RI;
    enIrqRegistration(&stcIrqRegiCfg);
    NVIC_SetPriority(stcIrqRegiCfg.enIRQn, DDL_IRQ_PRIORITY_02);
    NVIC_ClearPendingIRQ(stcIrqRegiCfg.enIRQn);
    NVIC_EnableIRQ(stcIrqRegiCfg.enIRQn);

    /* Set USART RX error IRQ */
    stcIrqRegiCfg.enIRQn = Int008_IRQn;
    stcIrqRegiCfg.pfnCallback = &Usart4recErrIrqCallback;
    stcIrqRegiCfg.enIntSrc = INT_USART4_EI;
    enIrqRegistration(&stcIrqRegiCfg);
    NVIC_SetPriority(stcIrqRegiCfg.enIRQn, DDL_IRQ_PRIORITY_05);
    NVIC_ClearPendingIRQ(stcIrqRegiCfg.enIRQn);
    NVIC_EnableIRQ(stcIrqRegiCfg.enIRQn);
    /*Enable RX && RX interupt function*/
    USART_FuncCmd(M4_USART4, UsartRx, Enable);
    USART_FuncCmd(M4_USART4, UsartTx, Enable);
    USART_FuncCmd(M4_USART4, UsartRxInt, Enable);
    return  0;;
}

void uart1_Tx(uint8 c)   //通过串口1把数据C发送出去
{
    USART_SendData(M4_USART1, c);
}

void uart2_Tx(uint8 c)   //通过串口2把数据C发送出去
{
    USART_SendData(M4_USART2, c);
}

void uart3_Tx(uint8 c)   //通过串口3把数据C发送出去
{
    USART_SendData(M4_USART3, c);
}

void uart4_Tx(uint8 c)   //通过串口4把数据C发送出去
{
    USART_SendData(M4_USART4, c);
}

void uart_reloaddmarxptr(int uid)
{
	M4_USART_TypeDef *M4_USART_ID;
  //  uint8 ch;
	if (uid == 0)
	{
		M4_USART_ID = M4_USART1;
      (0xff&USART_RecData(M4_USART_ID));//DMA溢出后要做这一步恢复
		uart_DmaRX_Init(2, M4_USART_ID, EVT_USART1_RI, Int001_IRQn, 
			DDL_IRQ_PRIORITY_00, MAX_UART0_BUF_SIZE, gUart0RecvBuf);
	}
	else if (uid == 1)
	{
		M4_USART_ID = M4_USART2;
		(0xff&USART_RecData(M4_USART_ID));
		uart_DmaRX_Init(1, M4_USART_ID, EVT_USART2_RI, Int005_IRQn, 
			DDL_IRQ_PRIORITY_01, MAX_UART1_BUF_SIZE, gUart1RecvBuf);
	}
	else if (uid == 2)
	{
		M4_USART_ID = M4_USART3;
		(0xff&USART_RecData(M4_USART_ID));
		uart_DmaRX_Init(1, M4_USART_ID, EVT_USART3_RI, Int005_IRQn, 
			DDL_IRQ_PRIORITY_01, MAX_UART2_BUF_SIZE, gUart2RecvBuf);
	}
	
    
    USART_ClearStatus(M4_USART_ID, UsartFrameErr);
    USART_ClearStatus(M4_USART_ID, UsartParityErr);
    USART_ClearStatus(M4_USART_ID, UsartOverrunErr);
    /*Enable RX && RX interupt function*/
    USART_FuncCmd(M4_USART_ID, UsartRx, Enable);
}


int hc32f460_uart_get_bytes_cnt(int uartid, int isrdma)
{
    uint16 rec;
    if(uartid==0)
    {
        rec=Read_DMA_Cnt(M4_DMA2,DmaCh0);
        if(rec<=MAX_UART0_BUF_SIZE)
        {
            return  (MAX_UART0_BUF_SIZE - rec);
        }
        else
            return 0;
    }
    else if(uartid==1)
    {
		 if (isrdma == 0)
			return uart2reccount;
		 else
		 {
			  rec = Read_DMA_Cnt(M4_DMA1, DmaCh0);
			  if(rec <= MAX_UART1_BUF_SIZE)
					return (MAX_UART1_BUF_SIZE - rec);
			  else
					return 0;			 
		 }
    }
    else if(uartid==2)
    {
		 if (isrdma == 0)
			 return uart3reccount;
		 else
		 {
			rec = Read_DMA_Cnt(M4_DMA1,DmaCh0);
			if(rec <= MAX_UART2_BUF_SIZE)
				return (MAX_UART2_BUF_SIZE - rec);
			else
				return 0;
		 }
    }
    else if(uartid==3)
    {
        return uart4reccount;
    }
    else   
		 return 0;
}



int uart_send(int s, const void *buf, uint32 len, int t485)   //通过串口把数据发送出去
{
	uint8 *dat = (uint8*)buf;
   uint16_t i;
	
	if(s == 3 && t485 == 1)
		RS485_set_send();
	
   for(i = 0; i < len; i++)
   {   
		if (s == 0)
			USART_SendData(M4_USART1, dat[i]);
      else if(s == 1)
			USART_SendData(M4_USART2, dat[i]);
      else if(s == 2)
			USART_SendData(M4_USART3, dat[i]);
      else if(s == 3)
			USART_SendData(M4_USART4, dat[i]);		
   }
	 
	if(s == 3 && t485 == 1)
		RS485_set_rec();
	
	return len;
}

int hc32f460_uart_clear_buf(int uartid, int isrdma)
{
    if(uartid==0)
    {
        uart_reloaddmarxptr(uartid);
        return 0;
    }
    else if(uartid==1)
    {
		 if (isrdma == 0)
			uart2reccount = 0;
		 else
			 uart_reloaddmarxptr(uartid);
       return 0;
    }
    else if(uartid==2)
    {
		 if (isrdma == 0)
			 uart3reccount = 0;
		 else
			uart_reloaddmarxptr(uartid);
       return 0;
    }
    else if(uartid==3)
    {
        uart4reccount=0;
        return 0;
    }
    else  
		 return -1;
}

int  hc32f460_init_uart_close(int uartid)
{
    if(uartid==0)
    {
        USART_FuncCmd(M4_USART1, UsartRx, Disable);
        USART_FuncCmd(M4_USART1, UsartTx, Disable);
        return 0;
    }
    else if(uartid==1)
    {
        USART_FuncCmd(M4_USART2, UsartRx, Disable);
        USART_FuncCmd(M4_USART2, UsartTx, Disable);
        return 0;
    }
    else if(uartid==2)
    {
        USART_FuncCmd(M4_USART3, UsartRx, Disable);
        USART_FuncCmd(M4_USART3, UsartTx, Disable);
        return 0;
    }
    else if(uartid==3)
    {
        USART_FuncCmd(M4_USART4, UsartRx, Disable);
        USART_FuncCmd(M4_USART4, UsartTx, Disable);
        return 0;
    }
    else
	 {
		 TRACE("hc32f460_init_uart_close err: invalid uartid\n");
		 return -1;
	 }
}

void uart_err_clear(int s)
{
	commonUartParaLocal *uartpara = &gUartParams[s];
	uartpara->uart_head = 0;
}

int uart_recv(int s, void *buf, uint32 len)
{
	int recvLen=0;
	commonUartParaLocal *uartpara = &gUartParams[s];
	if(uartpara->isOpen == 0)
		return -1;

	if (uartpara->uart_head == uartpara->uart_tail)
		return 0;
	else
	{
//		TRACE("start uart_tail:%d, uart_head:%d, len:%d\n", uartpara->uart_tail, uartpara->uart_head, len);
		if (uartpara->uart_tail > uartpara->uart_head)
		{
			recvLen = uartpara->uart_tail - uartpara->uart_head;
			if(recvLen > len)
				recvLen = len;
			memcpy_byb(buf, uartpara->recvbuf+uartpara->uart_head, recvLen);		
			uartpara->uart_head += recvLen;
//			printf("00000  recvLen:%d\n", recvLen);
		}
		else
		{
			recvLen = uartpara->recvbufsize - uartpara->uart_head;
			if(recvLen > len)
				recvLen = len;

			memcpy_byb(buf, (char *)uartpara->recvbuf+uartpara->uart_head, recvLen);
			uartpara->uart_head += recvLen;
//			printf("11111  recvLen:%d\n", recvLen);
			if (recvLen < len)
			{
				int band2len = len - recvLen;
				if (uartpara->uart_tail <= band2len)
					band2len = uartpara->uart_tail;
				memcpy_byb((char *)buf+recvLen, (char *)uartpara->recvbuf, band2len);
				recvLen += band2len;
				uartpara->uart_head = band2len;
//				printf("11111  band2len:%d\n", band2len);
			}
		}
	}
	
//	TRACE("end uart1_tail:%d, uart1_head:%d, len:%d\n", uart1_tail, uart1_head, len);
	/*
	for (i = 0; i < recvLen; ++i)
		printf("%02X ", ((char*)buf)[i]);
	printf("\n");
	*/
	if (uartpara->uart_head >= uartpara->recvbufsize)
		uartpara->uart_head = 0;
	return recvLen;
}


