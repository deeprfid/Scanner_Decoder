#ifndef _uart_H_
#define _uart_H_
#include "driverconfig.h"
#include "type.h"
#include "timer.h"
#include "hc32f46_driver.h"


#define MAX_UART0_BUF_SIZE 768
//#define MAX_UART1_BUF_SIZE 1536
//#define MAX_UART1_BUF_SIZE 6656
#if IS_RTOS2_SUPPORT
#define MAX_UART1_BUF_SIZE 3072
#else
#define MAX_UART1_BUF_SIZE 65528
#endif
#define MAX_UART2_BUF_SIZE 2048
#define MAX_UART3_BUF_SIZE 1536


extern uint8 gUart0RecvBuf[];
extern uint8 *gUart1RecvBuf;
extern uint8 gUart2RecvBuf[];
extern uint8 *gUart3RecvBuf;

extern volatile uint16_t  uart2reccount;
extern volatile uint16_t  uart3reccount;
extern volatile uint16_t  uart4reccount;

typedef struct
{
	uint32 baud;
	uint8 parity;
	uint8 databits;
	uint8 stopbits;
	uint8 flowctol;
	uint8 *recvbuf;
	uint16 buflen;
	uint8 isrdma;
	uint8 t485;
	int uartid;
} uart_cfg_para_st;

int uart1_Init(uart_cfg_para_st *ucpst);
int uart2_Init(uart_cfg_para_st *ucpst);
int uart3_Init(uart_cfg_para_st *ucpst);
int uart4_Init(uart_cfg_para_st *ucpst);

void uart1_Tx(uint8 c);
void uart2_Tx(uint8 c);
void uart3_Tx(uint8 c);
void uart4_Tx(uint8 c);


void uart3_CmdRevStateClr(void);

void uart_reloaddmarxptr(int uid);


int  hc32f460_uart_clear_buf(int uartid, int isrdma);
int  hc32f460_init_uart_close(int uartid);

int hc32f460_uart_get_bytes_cnt(int uartid, int isrdma);

int hc32f460_uart_init(uart_cfg_para_st *ucpst);

typedef struct
{
	uint8	isOpen;
	uint8 *recvbuf;
	uint16 recvbufsize;
	uint16 uart_head;
	uint16 uart_tail;
	commonUartPara basepara;
} commonUartParaLocal;

int uart_recv(int s, void *buf, uint32 len);
int uart_send(int s, const void *buf, uint32 len, int t485);


#endif

