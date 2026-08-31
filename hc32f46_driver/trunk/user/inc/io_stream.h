#ifndef _IO_STREAM_H_
#define _IO_STREAM_H_
#include "type.h"
#include "timer.h"
#include "hc32f46_driver.h"


typedef struct
{
	uint8 *recvbuf;
	uint16 recvbufsize;
volatile	uint16 usb_head;
volatile	uint16 usb_tail;
	uint8	isBlock;
	int	timeout;
} commonUsbParaLocal;

extern commonUsbParaLocal gUsbParams[2];

#define USB_COMPO_RXBUF_LEN 1536
extern uint8 cdc_rx_buf[];
extern uint8 cus_hid_rx_buf[];

#endif




