#include <stdio.h>
#include "uart.h"


/*串口DMA分配
uart1   RFID通信    DMA2  ch0
uart2   扩展接口    DMA1  ch1
uart3   串口        DMA1  ch0
uart4   485         DMA1  ch2
*/

RX_DMA_BUFFER  rxbuffers[4]; //4块DMA 存储区 512Byte X4
PRX_DMA_BUFFER uart1_prx_front= &rxbuffers[0];
PRX_DMA_BUFFER uart2_prx_front= &rxbuffers[1];
PRX_DMA_BUFFER uart3_prx_front= &rxbuffers[2];
PRX_DMA_BUFFER uart4_prx_front= &rxbuffers[3];

uint8_t SendBuff[512];
unsigned int FramRevTimeOutFlag;
unsigned int FramRevTimeStart;
unsigned char CmdRevStart;
unsigned char CmdRevFinish;
unsigned short LoseDatLen;

#define CheUartId \
	uartID = uartID - 100; \
	do { \
		if (uartID < 1 || uartID > 4) \
		{ \
			printf("invalid uartID\n");\
			return -1; \
		} \
	} while (0) \
	 
	

volatile uint8   DMAsend=0;       //DMA是否发送标志
volatile uint8   useDMA_uartID=0; //当前占用DMA发送的串口ID


int hc32f460_uart_init(uint8  uartID,uint32 u32Baudrate)
{
//	CheUartId;
	if(uartID==1)       
		uart1_Init(u32Baudrate,uart1_prx_front->buffer_data,RX_BUFFER_LENGTH);
  else if(uartID==2)  
		uart2_Init(u32Baudrate,uart2_prx_front->buffer_data,RX_BUFFER_LENGTH);
  else if(uartID==3)  
		uart3_Init(u32Baudrate,uart3_prx_front->buffer_data,RX_BUFFER_LENGTH);
  else if(uartID==4)  uart4_Init(u32Baudrate,uart4_prx_front->buffer_data,RX_BUFFER_LENGTH);
	
  return 0;
}

int uart1_Init(uint32 u32Baudrate,uint8 *addr,uint16 bufflen)//串口初始化配置函数，即是上电后的配置函数,配置成功返回0，不成功返回1
{
    uint8_t ch;
    USART_FuncCmd(M4_USART1, UsartRx, Disable);
    USART_FuncCmd(M4_USART1, UsartTx, Disable);
    USART_FuncCmd(M4_USART1, UsartRxInt, Disable);

    const stc_usart_uart_init_t stcInitCfg =
    {
        UsartIntClkCkNoOutput, //时钟不输出
        (u32Baudrate<115200)?UsartClkDiv_64:UsartClkDiv_1, //波特率分频
        UsartDataBits8,//8bits
        UsartDataLsbFirst,//LSB优先
        UsartOneStopBit,//1bit 停止位
        UsartParityNone,//无奇偶校验
        UsartSamleBit8, //8bit采样周期
        UsartStartBitFallEdge,//下降沿开始
        UsartRtsEnable,      //rts en
    };

    /* Initialize USART IO  Disable 禁止副功能 */
    PORT_SetFunc(PortA,Pin03, Func_Usart1_Rx, Disable);
    PORT_SetFunc(PortA,Pin02, Func_Usart1_Tx, Disable);

    /* Initialize UART */
    USART_UART_Init(M4_USART1, &stcInitCfg);
    /* Set baudrate */
    USART_SetBaudrate(M4_USART1, u32Baudrate);

    //DMA初始化
    ch= (0xff&USART_RecData(M4_USART1));//DMA溢出后要做这一步恢复
    uart1_DmaTX_Init();
    uart1_DmaRX_Init(bufflen,addr);

    USART_ClearStatus(M4_USART1, UsartFrameErr);
    USART_ClearStatus(M4_USART1, UsartParityErr);
    USART_ClearStatus(M4_USART1, UsartOverrunErr);
    /*Enable RX && RX interupt function*/
    USART_FuncCmd(M4_USART1, UsartRx, Enable);
    USART_FuncCmd(M4_USART1, UsartTx, Enable);
    uart1_prx_front->length=0;
    return  0;
}

int uart2_Init(uint32 u32Baudrate,uint8 *addr,uint16 bufflen)//串口初始化配置函数，即是上电后的配置函数,配置成功返回0，不成功返回1
{
    uint8_t ch;
    USART_FuncCmd(M4_USART2, UsartRx, Disable);
    USART_FuncCmd(M4_USART2, UsartTx, Disable);
    USART_FuncCmd(M4_USART2, UsartRxInt, Disable);

    const stc_usart_uart_init_t stcInitCfg =
    {
        UsartIntClkCkNoOutput, //时钟不输出
        (u32Baudrate<115200)?UsartClkDiv_64:UsartClkDiv_1, //波特率分频
        UsartDataBits8,//8bits
        UsartDataLsbFirst,//LSB优先
        UsartOneStopBit,//1bit 停止位
        UsartParityNone,//无奇偶校验
        UsartSamleBit8, //8bit采样周期
        UsartStartBitFallEdge,//下降沿开始
        UsartRtsEnable,      //rts en
    };


    /* Initialize USART IO  Disable 禁止副功能 */
    PORT_SetFunc(PortA,Pin01, Func_Usart2_Rx, Disable);
    PORT_SetFunc(PortA,Pin00, Func_Usart2_Tx, Disable);

    /* Initialize UART */
    USART_UART_Init(M4_USART2, &stcInitCfg);
    /* Set baudrate */
    USART_SetBaudrate(M4_USART2, u32Baudrate);

    //DMA初始化
    ch= (0xff&USART_RecData(M4_USART2));//DMA溢出后要做这一步恢复
    uart2_DmaTX_Init();
    uart2_DmaRX_Init(bufflen,addr);

    USART_ClearStatus(M4_USART2, UsartFrameErr);
    USART_ClearStatus(M4_USART2, UsartParityErr);
    USART_ClearStatus(M4_USART2, UsartOverrunErr);
    /*Enable RX && RX interupt function*/
    USART_FuncCmd(M4_USART2, UsartRx, Enable);
    USART_FuncCmd(M4_USART2, UsartTx, Enable);
    uart2_prx_front->length=0;
    return  0;
}

int uart3_Init(uint32 u32Baudrate,uint8 *addr,uint16 bufflen)//串口初始化配置函数，即是上电后的配置函数,配置成功返回0，不成功返回1
{   uint8_t ch;
    USART_FuncCmd(M4_USART3, UsartRx, Disable);
    USART_FuncCmd(M4_USART3, UsartTx, Disable);
    USART_FuncCmd(M4_USART3, UsartRxInt, Disable);

    const stc_usart_uart_init_t stcInitCfg =
    {
        UsartIntClkCkNoOutput, //时钟不输出
        (u32Baudrate<115200)?UsartClkDiv_64:UsartClkDiv_1, //波特率分频
        UsartDataBits8,//8bits
        UsartDataLsbFirst,//LSB优先
        UsartOneStopBit,//1bit 停止位
        UsartParityNone,//无奇偶校验
        UsartSamleBit8, //8bit采样周期
        UsartStartBitFallEdge,//下降沿开始
        UsartRtsEnable,      //rts en
    };


    /* Initialize USART IO  Disable 禁止副功能 */
    PORT_SetFunc(PortB,Pin06, Func_Usart3_Rx, Disable);
    PORT_SetFunc(PortB,Pin07, Func_Usart3_Tx, Disable);

    /* Initialize UART */
    USART_UART_Init(M4_USART3, &stcInitCfg);
    /* Set baudrate */
    USART_SetBaudrate(M4_USART3, u32Baudrate);

    //DMA初始化
    ch= (0xff&USART_RecData(M4_USART3));//DMA溢出后要做这一步恢复
    uart3_DmaTX_Init();
    uart3_DmaRX_Init(bufflen,addr);

    USART_ClearStatus(M4_USART3, UsartFrameErr);
    USART_ClearStatus(M4_USART3, UsartParityErr);
    USART_ClearStatus(M4_USART3, UsartOverrunErr);
    /*Enable RX && RX interupt function*/
    USART_FuncCmd(M4_USART3, UsartRx, Enable);
    USART_FuncCmd(M4_USART3, UsartTx, Enable);
    uart3_prx_front->length=0;
    return  0;
}


int uart4_Init(uint32 u32Baudrate,uint8 *addr,uint16 bufflen)//串口初始化配置函数，即是上电后的配置函数,配置成功返回0，不成功返回1
{
    uint8_t ch;
    USART_FuncCmd(M4_USART4, UsartRx, Disable);
    USART_FuncCmd(M4_USART4, UsartTx, Disable);
    USART_FuncCmd(M4_USART4, UsartRxInt, Disable);

    const stc_usart_uart_init_t stcInitCfg =
    {
        UsartIntClkCkNoOutput, //时钟不输出
        (u32Baudrate<115200)?UsartClkDiv_64:UsartClkDiv_1, //波特率分频
        UsartDataBits8,//8bits
        UsartDataLsbFirst,//LSB优先
        UsartOneStopBit,//1bit 停止位
        UsartParityNone,//无奇偶校验
        UsartSamleBit8, //8bit采样周期
        UsartStartBitFallEdge,//下降沿开始
        UsartRtsEnable,      //rts en
    };


    /* Initialize USART IO  Disable 禁止副功能 */
    PORT_SetFunc(PortB,Pin14, Func_Usart4_Rx, Disable);
    PORT_SetFunc(PortB,Pin13, Func_Usart4_Tx, Disable);

    /* Initialize UART */
    USART_UART_Init(M4_USART4, &stcInitCfg);
    /* Set baudrate */
    USART_SetBaudrate(M4_USART4, u32Baudrate);

    //DMA初始化
    ch= (0xff&USART_RecData(M4_USART4));//DMA溢出后要做这一步恢复
    uart4_DmaTX_Init();
    uart4_DmaRX_Init(bufflen,addr);

    USART_ClearStatus(M4_USART4, UsartFrameErr);
    USART_ClearStatus(M4_USART4, UsartParityErr);
    USART_ClearStatus(M4_USART4, UsartOverrunErr);
    /*Enable RX && RX interupt function*/
    USART_FuncCmd(M4_USART4, UsartRx, Enable);
    USART_FuncCmd(M4_USART4, UsartTx, Enable);
    uart4_prx_front->length=0;
    return  0;;
}






void uart1_DmaRX_Init(uint16_t cont,uint8_t *recadd) //DMA2 ch0
{
    stc_dma_config_t stcDmaInit;
    /* Enable DMA. */
    DMA_Cmd(M4_DMA2,Enable);

    /* Initialize DMA. */
    MEM_ZERO_STRUCT(stcDmaInit);
    stcDmaInit.u16BlockSize = 1u; /* 1 block */
    stcDmaInit.u16TransferCnt=cont;
    stcDmaInit.u32SrcAddr = ((uint32_t)(&M4_USART1->DR)+2ul); /* Set source address. */
    stcDmaInit.u32DesAddr=(uint32_t)uart1_prx_front->buffer_data;
    stcDmaInit.stcDmaChCfg.enSrcInc = AddressFix;  /* Set source address mode. */
    stcDmaInit.stcDmaChCfg.enDesInc = AddressIncrease;  /* Set destination address mode. */
    stcDmaInit.stcDmaChCfg.enIntEn = Disable;       /* Disable interrupt. */
    stcDmaInit.stcDmaChCfg.enTrnWidth = Dma8Bit;   /* Set data width 8bit. */
    DMA_InitChannel(M4_DMA2, DmaCh0, &stcDmaInit);
    DMA_ChannelCmd(M4_DMA2, DmaCh0, Enable);

    /* Enable PTDIS(AOS) clock*/
    PWC_Fcg0PeriphClockCmd(PWC_FCG0_PERIPH_PTDIS,Enable);
    DMA_SetTriggerSrc(M4_DMA2, DmaCh0, EVT_USART1_RI);

}
void uart2_DmaRX_Init(uint16_t cont,uint8_t *recadd) //DMA1 ch1
{
    stc_dma_config_t stcDmaInit;
    /* Enable DMA. */
    DMA_Cmd(M4_DMA1,Enable);

    /* Initialize DMA. */
    MEM_ZERO_STRUCT(stcDmaInit);
    stcDmaInit.u16BlockSize = 1u; /* 1 block */
    stcDmaInit.u16TransferCnt=cont;
    stcDmaInit.u32SrcAddr = ((uint32_t)(&M4_USART2->DR)+2ul); /* Set source address. */
    stcDmaInit.u32DesAddr=(uint32_t)uart2_prx_front->buffer_data;
    stcDmaInit.stcDmaChCfg.enSrcInc = AddressFix;  /* Set source address mode. */
    stcDmaInit.stcDmaChCfg.enDesInc = AddressIncrease;  /* Set destination address mode. */
    stcDmaInit.stcDmaChCfg.enIntEn = Disable;       /* Disable interrupt. */
    stcDmaInit.stcDmaChCfg.enTrnWidth = Dma8Bit;   /* Set data width 8bit. */
    DMA_InitChannel(M4_DMA1, DmaCh1, &stcDmaInit);
    DMA_ChannelCmd(M4_DMA1, DmaCh1, Enable);

    /* Enable PTDIS(AOS) clock*/
    PWC_Fcg0PeriphClockCmd(PWC_FCG0_PERIPH_PTDIS,Enable);
    DMA_SetTriggerSrc(M4_DMA1, DmaCh1, EVT_USART2_RI);
}


void uart3_DmaRX_Init(uint16_t cont,uint8_t *recadd)//DMA1 ch0
{
    stc_dma_config_t stcDmaInit;
    /* Enable DMA. */
    DMA_Cmd(M4_DMA1,Enable);

    /* Initialize DMA. */
    MEM_ZERO_STRUCT(stcDmaInit);
    stcDmaInit.u16BlockSize = 1u; /* 1 block */
    stcDmaInit.u16TransferCnt=cont;
    stcDmaInit.u32SrcAddr = ((uint32_t)(&M4_USART3->DR)+2ul); /* Set source address. */
    stcDmaInit.u32DesAddr=(uint32_t)uart3_prx_front->buffer_data;
    stcDmaInit.stcDmaChCfg.enSrcInc = AddressFix;  /* Set source address mode. */
    stcDmaInit.stcDmaChCfg.enDesInc = AddressIncrease;  /* Set destination address mode. */
    stcDmaInit.stcDmaChCfg.enIntEn = Disable;       /* Disable interrupt. */
    stcDmaInit.stcDmaChCfg.enTrnWidth = Dma8Bit;   /* Set data width 8bit. */
    DMA_InitChannel(M4_DMA1, DmaCh0, &stcDmaInit);
    DMA_ChannelCmd(M4_DMA1, DmaCh0, Enable);

    /* Enable PTDIS(AOS) clock*/
    PWC_Fcg0PeriphClockCmd(PWC_FCG0_PERIPH_PTDIS,Enable);
    DMA_SetTriggerSrc(M4_DMA1, DmaCh0, EVT_USART3_RI);
}


void uart4_DmaRX_Init(uint16_t cont,uint8_t *recadd)//DMA1 ch2
{
    stc_dma_config_t stcDmaInit;
    /* Enable DMA. */
    DMA_Cmd(M4_DMA1,Enable);

    /* Initialize DMA. */
    MEM_ZERO_STRUCT(stcDmaInit);
    stcDmaInit.u16BlockSize = 1u; /* 1 block */
    stcDmaInit.u16TransferCnt=cont;
    stcDmaInit.u32SrcAddr = ((uint32_t)(&M4_USART4->DR)+2ul); /* Set source address. */
    stcDmaInit.u32DesAddr=(uint32_t)uart4_prx_front->buffer_data;
    stcDmaInit.stcDmaChCfg.enSrcInc = AddressFix;  /* Set source address mode. */
    stcDmaInit.stcDmaChCfg.enDesInc = AddressIncrease;  /* Set destination address mode. */
    stcDmaInit.stcDmaChCfg.enIntEn = Disable;       /* Disable interrupt. */
    stcDmaInit.stcDmaChCfg.enTrnWidth = Dma8Bit;   /* Set data width 8bit. */
    DMA_InitChannel(M4_DMA1, DmaCh2, &stcDmaInit);
    DMA_ChannelCmd(M4_DMA1, DmaCh2, Enable);

    /* Enable PTDIS(AOS) clock*/
    PWC_Fcg0PeriphClockCmd(PWC_FCG0_PERIPH_PTDIS,Enable);
    DMA_SetTriggerSrc(M4_DMA1, DmaCh2, EVT_USART4_RI);


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

int uart1_Change_bps(uint32 u32Baudrate)
{
    uart1_Init(u32Baudrate,uart1_prx_front->buffer_data,RX_BUFFER_LENGTH);
    return  0;
}

int uart2_Change_bps(uint32 u32Baudrate)
{
    uart2_Init(u32Baudrate,uart2_prx_front->buffer_data,RX_BUFFER_LENGTH);
    return  0;
}



int uart3_Change_bps(uint32 u32Baudrate)
{
    uart3_Init(u32Baudrate,uart3_prx_front->buffer_data,RX_BUFFER_LENGTH);
    return  0;
}

int uart4_Change_bps(uint32 u32Baudrate)
{
    uart4_Init(u32Baudrate,uart4_prx_front->buffer_data,RX_BUFFER_LENGTH);
    return  0;
}


void uart1_reloaddmarxptr(void) //清缓冲区，只是指针指到0，内容没清，DMA 获取长度异常后一定调用这个函数
{
    uint8 ch;
    ch= (0xff&USART_RecData(M4_USART1));//DMA溢出后要做这一步恢复
    uart1_DmaRX_Init(RX_BUFFER_LENGTH,uart1_prx_front->buffer_data);
    USART_ClearStatus(M4_USART1, UsartFrameErr);
    USART_ClearStatus(M4_USART1, UsartParityErr);
    USART_ClearStatus(M4_USART1, UsartOverrunErr);
    /*Enable RX && RX interupt function*/
    USART_FuncCmd(M4_USART1, UsartRx, Enable);
    uart1_prx_front->length=0;
}

void uart2_reloaddmarxptr(void)//清缓冲区，只是指针指到0，内容没清，DMA 获取长度异常后一定调用这个函数
{
    uint8 ch;
    ch= (0xff&USART_RecData(M4_USART2));//DMA溢出后要做这一步恢复
    uart2_DmaRX_Init(RX_BUFFER_LENGTH,uart2_prx_front->buffer_data);
    USART_ClearStatus(M4_USART2, UsartFrameErr);
    USART_ClearStatus(M4_USART2, UsartParityErr);
    USART_ClearStatus(M4_USART2, UsartOverrunErr);
    /*Enable RX && RX interupt function*/
    USART_FuncCmd(M4_USART2, UsartRx, Enable);
    uart2_prx_front->length=0;
}


void uart3_reloaddmarxptr(void)////清缓冲区，只是指针指到0，内容没清，DMA 获取长度异常后一定调用这个函数
{
    uint8 ch;
    ch= (0xff&USART_RecData(M4_USART3));//DMA溢出后要做这一步恢复
    uart3_DmaRX_Init(RX_BUFFER_LENGTH,uart3_prx_front->buffer_data);
    USART_ClearStatus(M4_USART3, UsartFrameErr);
    USART_ClearStatus(M4_USART3, UsartParityErr);
    USART_ClearStatus(M4_USART3, UsartOverrunErr);
    /*Enable RX && RX interupt function*/
    USART_FuncCmd(M4_USART3, UsartRx, Enable);
    uart3_prx_front->length=0;
}


void uart4_reloaddmarxptr(void)////清缓冲区，只是指针指到0，内容没清，DMA 获取长度异常后一定调用这个函数
{
    uint8 ch;
    ch= (0xff&USART_RecData(M4_USART4));//DMA溢出后要做这一步恢复
    uart4_DmaRX_Init(RX_BUFFER_LENGTH,uart4_prx_front->buffer_data);
    USART_ClearStatus(M4_USART4, UsartFrameErr);
    USART_ClearStatus(M4_USART4, UsartParityErr);
    USART_ClearStatus(M4_USART4, UsartOverrunErr);
    /*Enable RX && RX interupt function*/
    USART_FuncCmd(M4_USART4, UsartRx, Enable);
    uart4_prx_front->length=0;
}


void CmdRevStateClr(void)
{
    FramRevTimeOutFlag=0;
    FramRevTimeStart=0;
    CmdRevFinish=0;
    LoseDatLen=0;
    CmdRevStart=0;
}


static uint32 service_rxdma(uint8* buff, uint16* bufflen)
{   //*buff为获取数据缓冲区指针，*bufflen为要获取的字节数，返回的值为实际获取的字节数
    unsigned int locbytes_received = 0;
    unsigned char *pBuf = 0;
    unsigned int rcr=0;
    uint16 getdatlen=0;
    uint16 i=0;
    uint32 bps_vals=115200;
    uint32 regval=0;
    getdatlen=*bufflen;
    *bufflen=0;

    if ( (Set == USART_GetStatus(M4_USART3, UsartFrameErr)) || (Set == USART_GetStatus(M4_USART3, UsartParityErr)) || (Set == USART_GetStatus(M4_USART3, UsartOverrunErr)) )
    {
        USART_ClearStatus(M4_USART3, UsartFrameErr);
        USART_ClearStatus(M4_USART3, UsartParityErr);
        USART_ClearStatus(M4_USART3, UsartOverrunErr);

        flash_OemRead(0x08,&regval);
        if((regval==9600)||(regval==19200)||(regval==38400)||(regval==57600)||(regval==115200)||(regval==230400)||(regval==460800)||(regval==921600))
        {
            bps_vals=regval;
        }
        else
        {
            bps_vals=115200;
        }
        uart3_Init(bps_vals,uart3_prx_front->buffer_data,RX_BUFFER_LENGTH);
         return 0xffff;
    }
    rcr=Read_DMA_Cnt(M4_DMA1,DmaCh0);
    locbytes_received = RX_BUFFER_LENGTH - rcr - uart3_prx_front->length;//256-初始化时PRX_FRONT->LENGTH==0，该值指示已经获取保存了的数据数
    if(locbytes_received > RX_BUFFER_LENGTH)//512
    {
        flash_OemRead(0x08,&regval);
        if((regval==9600)||(regval==19200)||(regval==38400)||(regval==57600)||(regval==115200)||(regval==230400)||(regval==460800)||(regval==921600))
        {
            bps_vals=regval;
        }
        else
        {
            bps_vals=115200;
        }
        uart3_Init(bps_vals,uart3_prx_front->buffer_data,RX_BUFFER_LENGTH);

        return 0xffff;
    }
    if(locbytes_received)
    {
        pBuf = (unsigned char *)&uart3_prx_front->buffer_data[uart3_prx_front->length];
        if(locbytes_received<getdatlen)
        {
            for(i=0; i<locbytes_received; i++)
            {
                buff[i]=pBuf[i];
            }
            *bufflen=locbytes_received;
            uart3_prx_front->length=uart3_prx_front->length+locbytes_received;
        }
        else
        {
            for(i=0; i<getdatlen; i++)
            {
                buff[i]=pBuf[i];
            }
            *bufflen=getdatlen;
            uart3_prx_front->length=uart3_prx_front->length+getdatlen;

        }
    }
    return 0;
}




static int hostuart_rx(uint8* buff,uint16* bufflen,uint16 timeout_ms)
{   //如果TIMEOUT为0，则表示直接取已经接收到的数据
    uint32 flag=0;
    unsigned int startTickCnt=0;
    uint16 getdatlen=0,getptr=0;
    timer_Get_lltimer(&startTickCnt);
    getdatlen=*bufflen;

    //测试接收**************************************************

    //TransmitChar(0xf2);
    //TransmitChar(*bufflen);
    //TransmitChar(0xf2);
    //**********************************************************
    if(timeout_ms==0)
    {
        flag=service_rxdma(buff,&getdatlen);
        *bufflen=getdatlen;
        //测试接收**************************************************
        //if(getdatlen!=0)
        //{
        //  TransmitChar(0xf2);
        //  TransmitChar((getdatlen&0xff)>>8);
        //  TransmitChar(getdatlen);
        //  TransmitChar(flag);
        //  TransmitChar(0xf2);
        //}
        //**********************************************************
        return flag;
    }
    else
    {
        while(timer_Diff_ms(startTickCnt)<timeout_ms)
        {   SWDT_RefreshCounter();
            flag=service_rxdma(&buff[getptr],&getdatlen);
            if(flag==0)
            {
                if((getdatlen+getptr)>=*bufflen)
                {
                    return 0;
                }
                getptr=getptr+getdatlen;
                getdatlen= *bufflen-getptr;

            }
            else
            {
                *bufflen=0;
                return flag;
            }
        }
        *bufflen=getptr;
    }

    return 0;
}


int uart3_GetCmdFram(uint8* buff,uint16* bufflen,uint8 *flag)
{
    //如果*FLAG==0，则为判断是否有已有完整的命令帧 ,如果没有则接收已经接收到的数据
    //FLAG!=0,则表示一直在这个函数里等待直到接收到一个完整的命令帧；
    uint8 revflag=0;
    uint16 datlen=0,datlen_lastwanttorev=0;
    uint8 revstartflag=0,haverecdat=0;//2016-6-24-9:58
    //测试接收**************************************************
    //uint16 i=0;
    //TransmitChar(0xf1);
    //TransmitChar(*flag);
    //TransmitChar(0xf1);
    //**********************************************************
//  uint16 losedatlen=0;//因为通信过程中难免有可能串口出现抖动或者是上位机发来杂乱命令，
    //那么这些数据将丢弃掉，这里记住丢弃的数目，达到256则复位RX DMA
    //LoseDatLen=0;
    //2016-6-23-19:58---------------------------
    if(*flag==0)//这里预留在以后如果使用 一盘存到数据就向上发送的时候使用，那个时候就一边进行操作一边检测是否上位机发来停止命令
    {
        datlen=1;
        datlen_lastwanttorev=1;//保存上一次调用接收命令时想要接收的数量，以免当调用接收函数后未接收数据DATLEN被赋值为0时的处理错误
        *bufflen=0;
        CmdRevStateClr();
        //hostuart_macops->cpu.get_lltimer(&hostuart_macops->FramRevTimeStart);
        while(1)
        {   SWDT_RefreshCounter();
            revflag=hostuart_rx(&buff[*bufflen],&datlen,0);
            if((revflag==0)&&(datlen!=0)) //1.说明已接收了有用数据*************************************************************
            {
                timer_Get_lltimer(&FramRevTimeStart);//已接收到数据更新下时间
                //测试接收的数据*****************************************
                //TransmitChar(0xfE);
                //for(i=0;i<datlen;i++)
                //{
                //  TransmitChar(buff[*bufflen+i]);
                // }
                // TransmitChar(0xfE);
                //*******************************************************
                haverecdat=1;//2016-6-24-9:59

                if(*bufflen<2) //（1）命令头与命令长度接收
                {
                    if((revstartflag==0)&&(buff[0]==0xff))//(1.0)命令头初判断
                    {
                        revstartflag=1;
                        timer_Get_lltimer(&FramRevTimeStart);
                        *bufflen=1;
                        datlen=1;
                        datlen_lastwanttorev=1;
                    }
                    else if(revstartflag==0) //（1.1）命令头不对，重新接收命令头
                    {
                        LoseDatLen=LoseDatLen+1;
                        datlen=1;
                        datlen_lastwanttorev=1;
                    }
                    else 	//(1.2)M命令头0XFF已接收后 后2字节的接收处理
                    {
                        if(buff[1]<251)	 //(1.201)命令长度正确，则赋值命令串余下需要接收的长度
                        {
                            *bufflen=2;
                            datlen=buff[1]+3;
                            datlen_lastwanttorev=datlen;
                        }
                        else if(buff[1]==0xff) //(1.202)第一个0XFF命令头未误码，第2个0XFF为真正命令头，重新接收
                        {
                            LoseDatLen=LoseDatLen+1;
                            revstartflag=1;
                            timer_Get_lltimer(&FramRevTimeStart);
                            *bufflen=1;
                            datlen=1;
                            datlen_lastwanttorev=datlen;
                        }
                        else	 //数据是无效数据,从头开始接收
                        {
                            LoseDatLen=LoseDatLen+1;
                            datlen=1;
                            datlen_lastwanttorev=datlen;
                            *bufflen=0;
                            revstartflag=0;
                        }
                    }
                }
                else//（2）命令码与剩余数据与CRC接收*
                {
                    *bufflen=*bufflen+datlen;
                    datlen=buff[1]+5-(*bufflen);
                    datlen_lastwanttorev=datlen;
                    if(*bufflen>=(buff[1]+5))	//已完成一个命令帧的接收
                    {
                        *bufflen=buff[1]+5;
                        *flag=1;
                        uart3_reloaddmarxptr();
                        return 0;
                    }
                }

            }
            else if(revflag==0)
            {
                if(*bufflen==0)
                {
                    CmdRevStateClr(); //2016-6-24-10:08---------
                    if(haverecdat!=0)//2016-6-24-10:02---------
                    {
                        uart3_reloaddmarxptr();
                    }
                    return 1;
                }
                else
                {
                    datlen=datlen_lastwanttorev;
                }
            }
            else if(revflag!=0) //2.串口接收出现异常，串口已经初始化，所以所有数据初始化**************************************************
            {
                CmdRevStateClr();
                uart3_reloaddmarxptr();
                datlen=1;
                datlen_lastwanttorev=datlen;
                *bufflen=0;
                revstartflag=0;
                return 1;
            }
            if((revstartflag==1)&&(timer_Diff_ms(FramRevTimeStart)>250))//3.在50ms内还未收到下一个字节则丢弃数据包***********************************
            {   //这里后面还是做下处理，如果每个字节之间的间隔大于50MS则认为数据丢包//50 change to 250 for blueteeth-2019-7-19-11:23----
                CmdRevStateClr();
                uart3_reloaddmarxptr();
                datlen=1;
                datlen_lastwanttorev=datlen;
                *bufflen=0;
                revstartflag=0;
                return 1;
            }
            if(LoseDatLen>=255) //4.有太多的非正确命令码则重新复位DMA接收******************************************************************
            {
                CmdRevStateClr();
                uart3_reloaddmarxptr();
                datlen=1;
                datlen_lastwanttorev=datlen;
                *bufflen=0;
                revstartflag=0;
                return 1;
            }
            //reloaddmarxptr();
        }
    }
    else
    {
        datlen=1;
        datlen_lastwanttorev=1;//保存上一次调用接收命令时想要接收的数量，以免当调用接收函数后未接收数据DATLEN被赋值为0时的处理错误
        *bufflen=0;
        CmdRevStateClr();
        //reloaddmarxptr();
        //hostuart_macops->cpu.get_lltimer(&hostuart_macops->FramRevTimeStart);
        while(1)
        {   SWDT_RefreshCounter();
            revflag=hostuart_rx(&buff[*bufflen],&datlen,0);
            if((revflag==0)&&(datlen!=0)) //1.说明已接收了有用数据*************************************************************
            {
                timer_Get_lltimer(&FramRevTimeStart);//已接收到数据更新下时间
                //测试接收的数据*****************************************
                //TransmitChar(0xfE);
                //for(i=0;i<datlen;i++)
                //{
                //  TransmitChar(buff[*bufflen+i]);
                // }
                // TransmitChar(0xfE);
                //*******************************************************

                if(*bufflen<2) //（1）命令头与命令长度接收
                {
                    if((revstartflag==0)&&(buff[0]==0xff))//(1.0)命令头初判断
                    {
                        revstartflag=1;
                        timer_Get_lltimer(&FramRevTimeStart);
                        *bufflen=1;
                        datlen=1;
                        datlen_lastwanttorev=1;
                    }
                    else if(revstartflag==0) //（1.1）命令头不对，重新接收命令头
                    {
                        LoseDatLen=LoseDatLen+1;
                        datlen=1;
                        datlen_lastwanttorev=1;
                    }
                    else 	//(1.2)M命令头0XFF已接收后 后2字节的接收处理
                    {
                        if(buff[1]<251)	 //(1.201)命令长度正确，则赋值命令串余下需要接收的长度
                        {
                            *bufflen=2;
                            datlen=buff[1]+3;
                            datlen_lastwanttorev=datlen;
                        }
                        else if(buff[1]==0xff) //(1.202)第一个0XFF命令头未误码，第2个0XFF为真正命令头，重新接收
                        {
                            LoseDatLen=LoseDatLen+1;
                            revstartflag=1;
                            timer_Get_lltimer(&FramRevTimeStart);
                            *bufflen=1;
                            datlen=1;
                            datlen_lastwanttorev=datlen;
                        }
                        else	 //数据是无效数据,从头开始接收
                        {
                            LoseDatLen=LoseDatLen+1;
                            datlen=1;
                            datlen_lastwanttorev=datlen;
                            *bufflen=0;
                            revstartflag=0;
                        }
                    }
                }
                else//（2）命令码与剩余数据与CRC接收*
                {
                    *bufflen=*bufflen+datlen;
                    datlen=buff[1]+5-(*bufflen);
                    datlen_lastwanttorev=datlen;
                    if(*bufflen>=(buff[1]+5))	//已完成一个命令帧的接收
                    {
                        *bufflen=buff[1]+5;
                        *flag=1;
                        uart3_reloaddmarxptr();
                        return 0;
                    }
                }

            }
            else if(revflag==0)
            {
                datlen=datlen_lastwanttorev;
            }
            else if(revflag!=0) //2.串口接收出现异常，串口已经初始化，所以所有数据初始化**************************************************
            {   CmdRevStateClr();
                uart3_reloaddmarxptr();
                datlen=1;
                datlen_lastwanttorev=datlen;
                *bufflen=0;
                revstartflag=0;
            }
            if((revstartflag==1)&&(timer_Diff_ms(FramRevTimeStart)>250))//3.在50ms内还未收到下一个字节则丢弃数据包***********************************
            {   //这里后面还是做下处理，如果每个字节之间的间隔大于50MS则认为数据丢包//50 change to 250 for blueteeth-2019-7-19-11:28----
                CmdRevStateClr();
                uart3_reloaddmarxptr();
                datlen=1;
                datlen_lastwanttorev=datlen;
                *bufflen=0;
                revstartflag=0;
            }
            if(LoseDatLen>=255) //4.有太多的非正确命令码则重新复位DMA接收******************************************************************
            {
                CmdRevStateClr();
                uart3_reloaddmarxptr();
                datlen=1;
                datlen_lastwanttorev=datlen;
                *bufflen=0;
                revstartflag=0;
            }
            //reloaddmarxptr();
        }
    }

//  return 0;
//这里要做下处理，接收好一帧数据后要把DMA RX指针与计数复位下
}



void uart1_DmaTX_Init(void)   //DMA1  ch3
{
    stc_dma_config_t stcDmaInit;
    /* Enable DMA. */
    DMA_Cmd(M4_DMA1,Enable);

    /* Initialize DMA. */
    MEM_ZERO_STRUCT(stcDmaInit);
    stcDmaInit.u16BlockSize = 1u; /* 1 block */
    stcDmaInit.u16TransferCnt=256;                       //默认传输256次
    stcDmaInit.u32SrcAddr = (uint32_t)(SendBuff);        /* Set source address. */
    stcDmaInit.u32DesAddr = (uint32_t)(&M4_USART1->DR);     /* Set destination address. */
    stcDmaInit.stcDmaChCfg.enSrcInc = AddressIncrease;  /* Set source address mode. */
    stcDmaInit.stcDmaChCfg.enDesInc = AddressFix;  /* Set destination address mode. */
    stcDmaInit.stcDmaChCfg.enIntEn = Disable;       /* Disable interrupt. */
    stcDmaInit.stcDmaChCfg.enTrnWidth = Dma8Bit;   /* Set data width 8bit. */
    DMA_InitChannel(M4_DMA1, DmaCh3, &stcDmaInit);
    DMA_ChannelCmd(M4_DMA1, DmaCh3, Disable);

    /* Clear DMA flag. */
    DMA_ClearIrqFlag(M4_DMA1, DmaCh3, TrnCpltIrq);

    /* Enable PTDIS(AOS) clock*/
    PWC_Fcg0PeriphClockCmd(PWC_FCG0_PERIPH_PTDIS,Enable);
    DMA_SetTriggerSrc(M4_DMA1, DmaCh3, EVT_USART1_TI);
    //DMA_SetTriggerSrc(M4_DMA1, DmaCh0, EVT_AOS_STRG);//软件触发
    useDMA_uartID=1;
}


void uart2_DmaTX_Init(void)  //DMA1  ch3
{
    stc_dma_config_t stcDmaInit;

    /* Enable DMA. */
    DMA_Cmd(M4_DMA1,Enable);

    /* Initialize DMA. */
    MEM_ZERO_STRUCT(stcDmaInit);
    stcDmaInit.u16BlockSize = 1u; /* 1 block */
    stcDmaInit.u16TransferCnt=256;                       //默认传输256次
    stcDmaInit.u32SrcAddr = (uint32_t)(SendBuff);        /* Set source address. */
    stcDmaInit.u32DesAddr = (uint32_t)(&M4_USART2->DR);     /* Set destination address. */
    stcDmaInit.stcDmaChCfg.enSrcInc = AddressIncrease;  /* Set source address mode. */
    stcDmaInit.stcDmaChCfg.enDesInc = AddressFix;  /* Set destination address mode. */
    stcDmaInit.stcDmaChCfg.enIntEn = Disable;       /* Disable interrupt. */
    stcDmaInit.stcDmaChCfg.enTrnWidth = Dma8Bit;   /* Set data width 8bit. */
    DMA_InitChannel(M4_DMA1, DmaCh3, &stcDmaInit);
    DMA_ChannelCmd(M4_DMA1, DmaCh3, Disable);

    /* Clear DMA flag. */
    DMA_ClearIrqFlag(M4_DMA1, DmaCh3, TrnCpltIrq);

    /* Enable PTDIS(AOS) clock*/
    PWC_Fcg0PeriphClockCmd(PWC_FCG0_PERIPH_PTDIS,Enable);
    DMA_SetTriggerSrc(M4_DMA1, DmaCh3, EVT_USART2_TI);
    //DMA_SetTriggerSrc(M4_DMA1, DmaCh0, EVT_AOS_STRG);//软件触发
    useDMA_uartID=2;
}

void uart3_DmaTX_Init(void)  //DMA1  ch3
{
    stc_dma_config_t stcDmaInit;
    DMA_Cmd(M4_DMA1,Enable);
    /* Initialize DMA. */
    MEM_ZERO_STRUCT(stcDmaInit);
    stcDmaInit.u16BlockSize = 1u; /* 1 block */
    stcDmaInit.u16TransferCnt=256;                       //默认传输256次
    stcDmaInit.u32SrcAddr = (uint32_t)(SendBuff);        /* Set source address. */
    stcDmaInit.u32DesAddr = (uint32_t)(&M4_USART3->DR);     /* Set destination address. */
    stcDmaInit.stcDmaChCfg.enSrcInc = AddressIncrease;  /* Set source address mode. */
    stcDmaInit.stcDmaChCfg.enDesInc = AddressFix;  /* Set destination address mode. */
    stcDmaInit.stcDmaChCfg.enIntEn = Disable;       /* Disable interrupt. */
    stcDmaInit.stcDmaChCfg.enTrnWidth = Dma8Bit;   /* Set data width 8bit. */
    DMA_InitChannel(M4_DMA1, DmaCh3, &stcDmaInit);
    DMA_ChannelCmd(M4_DMA1, DmaCh3, Disable);

    /* Clear DMA flag. */
    DMA_ClearIrqFlag(M4_DMA1, DmaCh3, TrnCpltIrq);

    /* Enable PTDIS(AOS) clock*/
    PWC_Fcg0PeriphClockCmd(PWC_FCG0_PERIPH_PTDIS,Enable);
    DMA_SetTriggerSrc(M4_DMA1, DmaCh3, EVT_USART3_TI);

    useDMA_uartID=3;
}

void uart4_DmaTX_Init(void)  //DMA1  ch3
{
    stc_dma_config_t stcDmaInit;
    DMA_Cmd(M4_DMA1,Enable);
    /* Initialize DMA. */
    MEM_ZERO_STRUCT(stcDmaInit);
    stcDmaInit.u16BlockSize = 1u; /* 1 block */
    stcDmaInit.u16TransferCnt=256;                       //默认传输256次
    stcDmaInit.u32SrcAddr = (uint32_t)(SendBuff);        /* Set source address. */
    stcDmaInit.u32DesAddr = (uint32_t)(&M4_USART4->DR);     /* Set destination address. */
    stcDmaInit.stcDmaChCfg.enSrcInc = AddressIncrease;  /* Set source address mode. */
    stcDmaInit.stcDmaChCfg.enDesInc = AddressFix;  /* Set destination address mode. */
    stcDmaInit.stcDmaChCfg.enIntEn = Disable;       /* Disable interrupt. */
    stcDmaInit.stcDmaChCfg.enTrnWidth = Dma8Bit;   /* Set data width 8bit. */
    DMA_InitChannel(M4_DMA1, DmaCh3, &stcDmaInit);
    DMA_ChannelCmd(M4_DMA1, DmaCh3, Disable);

    /* Clear DMA flag. */
    DMA_ClearIrqFlag(M4_DMA1, DmaCh3, TrnCpltIrq);

    /* Enable PTDIS(AOS) clock*/
    PWC_Fcg0PeriphClockCmd(PWC_FCG0_PERIPH_PTDIS,Enable);
    DMA_SetTriggerSrc(M4_DMA1, DmaCh3, EVT_USART4_TI);
    useDMA_uartID=4;
}


int hc32f460_uart_get_bytes_cnt(int uartid,uint16 *len) // 返回1的时候，表示DMA溢出了,数据不能用, 返回0的时候 len为收到数据长度；
{
    uint16 rec;
    if(uartid==1)
    {
        if ( (Set == USART_GetStatus(M4_USART1, UsartFrameErr)) || (Set == USART_GetStatus(M4_USART1, UsartParityErr)) || (Set == USART_GetStatus(M4_USART1, UsartOverrunErr)) )
        {
            return 1;
        }
        else
        {
            rec=Read_DMA_Cnt(M4_DMA2,DmaCh0);
            if( (rec>12)&&(rec<=RX_BUFFER_LENGTH) )
            {   *len=RX_BUFFER_LENGTH - rec;
                return 0;
            }
            else
                return 1;
        }

    }
    else if(uartid==2)
    {
        if ( (Set == USART_GetStatus(M4_USART2, UsartFrameErr)) || (Set == USART_GetStatus(M4_USART2, UsartParityErr)) || (Set == USART_GetStatus(M4_USART2, UsartOverrunErr)) )
        {
            return 1;
        }
        else
        {
            rec=Read_DMA_Cnt(M4_DMA1,DmaCh1);
            if( (rec>12)&&(rec<=RX_BUFFER_LENGTH) )
            {
                *len=RX_BUFFER_LENGTH - rec;
                return 0;
            }
            else
                return 1;
        }

    }
    else if(uartid==3)
    {
        if ( (Set == USART_GetStatus(M4_USART3, UsartFrameErr)) || (Set == USART_GetStatus(M4_USART3, UsartParityErr)) || (Set == USART_GetStatus(M4_USART3, UsartOverrunErr)) )
        {
            return 1;
        }
        else
        {
            rec=Read_DMA_Cnt(M4_DMA1,DmaCh0);
            if( (rec>12)&&(rec<=RX_BUFFER_LENGTH) )
            {
                *len=RX_BUFFER_LENGTH - rec;
                return 0;
            }
            else
                return 1;
        }

    }
    else if(uartid==4)
    {
        if ( (Set == USART_GetStatus(M4_USART4, UsartFrameErr)) || (Set == USART_GetStatus(M4_USART4, UsartParityErr)) || (Set == USART_GetStatus(M4_USART4, UsartOverrunErr)) )
        {
            return 1;
        }
        else
        {
            rec=Read_DMA_Cnt(M4_DMA1,DmaCh2);
            if( (rec>12)&&(rec<=RX_BUFFER_LENGTH) )
            {
                *len=RX_BUFFER_LENGTH - rec;
                return 0;
            }
            else
                return 1;
        }

    }
    else   return 1;

}



void uart1_TXDMA_Enable(uint16_t trcnt)  //DMA1  ch3
{
    DMA_ClearIrqFlag(M4_DMA1, DmaCh3, TrnCpltIrq);
    USART_FuncCmd(M4_USART1, UsartTx, Disable);//tx txempty要同时开才有效，所以先关闭
    DMA_SetTransferCnt(M4_DMA1, DmaCh3, trcnt);
    DMA_SetSrcAddress(M4_DMA1, DmaCh3,(uint32_t)(SendBuff));
    DMA_ChannelCmd(M4_DMA1, DmaCh3,Enable);
    USART_FuncCmd(M4_USART1, UsartTxAndTxEmptyInt, Enable);
}

void uart2_TXDMA_Enable(uint16_t trcnt) //DMA1  ch3
{
    DMA_ClearIrqFlag(M4_DMA1, DmaCh3, TrnCpltIrq);
    USART_FuncCmd(M4_USART2, UsartTx, Disable);//tx txempty要同时开才有效，所以先关闭
    DMA_SetTransferCnt(M4_DMA1, DmaCh3, trcnt);
    DMA_SetSrcAddress(M4_DMA1, DmaCh3,(uint32_t)(SendBuff));
    DMA_ChannelCmd(M4_DMA1, DmaCh3,Enable);
    USART_FuncCmd(M4_USART2, UsartTxAndTxEmptyInt, Enable);
}


void uart3_TXDMA_Enable(uint16_t trcnt) //DMA1  ch3
{
    DMA_ClearIrqFlag(M4_DMA1, DmaCh3, TrnCpltIrq);
    USART_FuncCmd(M4_USART3, UsartTx, Disable);//tx txempty要同时开才有效，所以先关闭
    DMA_SetTransferCnt(M4_DMA1, DmaCh3, trcnt);
    DMA_SetSrcAddress(M4_DMA1, DmaCh3,(uint32_t)(SendBuff));
    DMA_ChannelCmd(M4_DMA1, DmaCh3,Enable);
    USART_FuncCmd(M4_USART3, UsartTxAndTxEmptyInt, Enable);
}


void uart4_TXDMA_Enable(uint16_t trcnt)  //DMA1  ch3
{
    DMA_ClearIrqFlag(M4_DMA1, DmaCh3, TrnCpltIrq);
    USART_FuncCmd(M4_USART4, UsartTx, Disable);//tx txempty要同时开才有效，所以先关闭
    DMA_SetTransferCnt(M4_DMA1, DmaCh3, trcnt);
    DMA_SetSrcAddress(M4_DMA1, DmaCh3,(uint32_t)(SendBuff));
    DMA_ChannelCmd(M4_DMA1, DmaCh3,Enable);
    USART_FuncCmd(M4_USART4, UsartTxAndTxEmptyInt, Enable);
}


int WaitDMAFree(void)  //一直等待直到DMA结束.要发送了数据才会才产生发送完成标志，否则一直等不到
{
    uint32 TimeCnt;
    timer_Get_lltimer(&TimeCnt);

    if( Read_DMA_Cnt(M4_DMA2,DmaCh0)<=12)
        uart1_reloaddmarxptr();
    if( Read_DMA_Cnt(M4_DMA1,DmaCh1)<=12)
        uart2_reloaddmarxptr();
    if( Read_DMA_Cnt(M4_DMA1,DmaCh0)<=12)
        uart3_reloaddmarxptr();
    if( Read_DMA_Cnt(M4_DMA1,DmaCh2)<=12)
        uart4_reloaddmarxptr();

    if(DMAsend==0) return 0;
    else
    {
        while(DMA_GetIrqFlag(M4_DMA1, DmaCh3, TrnCpltIrq)==0)
        {
            SWDT_RefreshCounter();
            if(timer_Diff_ms(TimeCnt)>400)
            {
                DMAsend=0;
                return 1;
            }
        }
        if(useDMA_uartID==1)
        {
            while (0u == M4_USART1->SR_f.TC);   //等待串口发送完成
        }
        else if (useDMA_uartID==2)
        {
            while (0u == M4_USART2->SR_f.TC);
        }
        else if (useDMA_uartID==3)
        {
            while (0u == M4_USART3->SR_f.TC);
        }
        else if (useDMA_uartID==4)
        {
            while (0u == M4_USART4->SR_f.TC);
        }
        DMA_ClearIrqFlag(M4_DMA1, DmaCh3, TrnCpltIrq);
        DMAsend=0;
        return 0;
    }

}


uint8_t ReadDMAState(void)//读取DMA状态，发送完毕返回1，没完毕返回0
{
    if(DMAsend==0)
    {
        return 1;
    }
    else
    {
        if(DMA_GetIrqFlag(M4_DMA1, DmaCh3, TrnCpltIrq)==1)
        {
            if(useDMA_uartID==1)
            {
                while (0u == M4_USART1->SR_f.TC);
            }
            else if(useDMA_uartID==2)
            {
                while (0u == M4_USART2->SR_f.TC);
            }
            else if(useDMA_uartID==3)
            {
                while (0u == M4_USART3->SR_f.TC);
            }
            else if(useDMA_uartID==4)
            {
                while (0u == M4_USART4->SR_f.TC);
            }

            DMA_ClearIrqFlag(M4_DMA1, DmaCh3, TrnCpltIrq);
            DMAsend=0;
            return 1;
        }
        else return 0;
    }
}

void TxBuf(uint8_t uartID,uint8_t *dat,uint16_t len,uint8_t dmaflag)   //通过串口2把数据发送出去,dmaflag=0时简单串口发送，！=0时DMA模式
{

    uint16_t i;
    if(dmaflag==0)//普通串口发送
    {
        WaitDMAFree();
        DMA_ChannelCmd(M4_DMA1, DmaCh3,Disable); //禁止串口DMA发送
        for(i=0; i<len; i++)
        {   if (uartID==1)     USART_SendData(M4_USART1, dat[i]);
            else if(uartID==2) USART_SendData(M4_USART2, dat[i]);
            else if(uartID==3) USART_SendData(M4_USART3, dat[i]);
            else if(uartID==4) USART_SendData(M4_USART4, dat[i]);
        }
    }
    else if( (len!=0) && (len<=512) )	//DMA模式
    {
        WaitDMAFree();
        memcpy(SendBuff,dat,len);
        if(useDMA_uartID!=uartID)
        {
            if(uartID==1)      uart1_DmaTX_Init();
            else if(uartID==2) uart2_DmaTX_Init();
            else if(uartID==3) uart3_DmaTX_Init();
            else if(uartID==4) uart4_DmaTX_Init();
        }
        if(uartID==1)
        {
            uart1_TXDMA_Enable(len);
            DMAsend=1;
        }
        else if(uartID==2)
        {
            uart2_TXDMA_Enable(len);
            DMAsend=1;
        }
        else if(uartID==3)
        {
            uart3_TXDMA_Enable(len);
            DMAsend=1;
        }
        else if(uartID==4)
        {
            uart4_TXDMA_Enable(len);
            DMAsend=1;
        }
    }
}

int hc32f460_uart_clear_buf(int uartid)
{
    if(uartid==1)
    {
        uart1_reloaddmarxptr();
        return 0;
    }
    else if(uartid==2)
    {
        uart2_reloaddmarxptr();
        return 0;
    }
    else if(uartid==3)
    {
        uart3_reloaddmarxptr();
        return 0;
    }
    else if(uartid==4)
    {
        uart4_reloaddmarxptr();
        return 0;
    }
    else  return 1;
}
int  hc32f460_init_uart_close(int uartid)
{
    if(uartid==1)
    {
        USART_FuncCmd(M4_USART1, UsartRx, Disable);
        USART_FuncCmd(M4_USART1, UsartTx, Disable);
        return 0;
    }
    else if(uartid==2)
    {
        USART_FuncCmd(M4_USART2, UsartRx, Disable);
        USART_FuncCmd(M4_USART2, UsartTx, Disable);
        return 0;
    }
    else if(uartid==3)
    {
        USART_FuncCmd(M4_USART3, UsartRx, Disable);
        USART_FuncCmd(M4_USART3, UsartTx, Disable);
        return 0;
    }
    else if(uartid==4)
    {
        USART_FuncCmd(M4_USART4, UsartRx, Disable);
        USART_FuncCmd(M4_USART4, UsartTx, Disable);
        return 0;
    }
    else  return 1;
}

