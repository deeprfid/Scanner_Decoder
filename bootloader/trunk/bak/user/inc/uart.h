#ifndef _uart_H_
#define _uart_H_
#include "type.h"
#include "irq.h"
#include "timer.h"
#include "hc32_ddl.h"
#include "flash.h"
//#include "handlemcu.h"


#define RX_BUFFER_LENGTH   512
extern uint8_t SendBuff[512];


typedef struct _RX_DMA_BUFFER
{
    uint8_t buffer_data[RX_BUFFER_LENGTH];
    unsigned int length;
} RX_DMA_BUFFER, *PRX_DMA_BUFFER;



extern PRX_DMA_BUFFER uart1_prx_front;
extern PRX_DMA_BUFFER uart2_prx_front;
extern PRX_DMA_BUFFER uart3_prx_front;
extern PRX_DMA_BUFFER uart4_prx_front;




int uart1_Init(uint32 u32Baudrate,uint8 *addr,uint16 bufflen);
int uart2_Init(uint32 u32Baudrate,uint8 *addr,uint16 bufflen);
int uart3_Init(uint32 u32Baudrate,uint8 *addr,uint16 bufflen);
int uart4_Init(uint32 u32Baudrate,uint8 *addr,uint16 bufflen);

void uart1_Tx(uint8 c);
void uart2_Tx(uint8 c);
void uart3_Tx(uint8 c);
void uart4_Tx(uint8 c);

int uart1_Change_bps(uint32 u32Baudrate);
int uart2_Change_bps(uint32 u32Baudrate);
int uart3_Change_bps(uint32 u32Baudrate);
int uart4_Change_bps(uint32 u32Baudrate);


int uart3_GetCmdFram(uint8* buff,uint16* bufflen,uint8 *flag);//接收完整数据帧函数，buff为存放数据帧的数组；
//bufflen为协议数据帧里的data length值，看M5E协议或SLR1100协议介绍的
//模块接收上位机协议命令格式（也即是接收到FF后的下个数据的值）；
//flag这个参数暂时不用理会，后续扩展用；该函数为接收到一个完整数据帧时
//才退返回出，且函数值为0即函数最后要return 0;

void uart1_DmaTX_Init(void);
void uart2_DmaTX_Init(void);
void uart3_DmaTX_Init(void);
void uart4_DmaTX_Init(void);

void uart1_DmaRX_Init(uint16_t cont,uint8_t *recadd);
void uart2_DmaRX_Init(uint16_t cont,uint8_t *recadd);
void uart3_DmaRX_Init(uint16_t cont,uint8_t *recadd);
void uart4_DmaRX_Init(uint16_t cont,uint8_t *recadd);

void uart1_TXDMA_Enable(uint16_t trcnt);
void uart2_TXDMA_Enable(uint16_t trcnt);
void uart3_TXDMA_Enable(uint16_t trcnt);
void uart4_TXDMA_Enable(uint16_t trcnt);

int  WaitDMAFree(void);

uint8_t ReadDMAState(void);

void TxBuf(uint8_t uartID,uint8_t *dat,uint16_t len,uint8_t dmaflag);


void uart3_CmdRevStateClr(void);

void uart1_reloaddmarxptr(void);
void uart2_reloaddmarxptr(void);
void uart3_reloaddmarxptr(void);
void uart4_reloaddmarxptr(void);

int  hc32f460_uart_clear_buf(int uartid);
int  hc32f460_init_uart_close(int uartid);

int  hc32f460_uart_get_bytes_cnt(int uartid,uint16 *len);
int  hc32f460_uart_init(uint8  uartID,uint32 u32Baudrate);

#define COMMON_INTERFACE_UART0	100 // uart0 
#define COMMON_INTERFACE_UART1	101 // uart1
#define COMMON_INTERFACE_UART2	102 // uart1
#define COMMON_INTERFACE_UART3	103 // uart1

#endif

