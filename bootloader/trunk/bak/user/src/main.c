#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "hc32f46_driver.h"
#include "hc32f46x_interrupts.h"


//0x1FFF8000
//0x2F000
/*
PendSV_Handler
SysTick_Handler
SVC_Handler
*/
//20026FF0

int main(void)
{
	int cnt = 0;
	int a = 0;
	int b = 10;
	int ret;
	int ret2;
	int listenSn;
	int connectSn;
	
	int outsk[2];
	unsigned char recvbuf[50];
		commonUartPara uart2Para;
	int timeout1 = -1;
	int timeout2 = 1000;

	uart2Para.isBlock	= O_BLOCK;
	uart2Para.isPrintf	= 1;
	uart2Para.baudrate	= 115200;
	uart2Para.timeout	= -1;
	

   RCC_Configuration();
   GPIO_Configuration();
   timer_Init();

	uart_open(COMMON_INTERFACE_UART2, &uart2Para);
	

		
	enIrqResign(Int006_IRQn);
//		TIMERA_IrqCmd(M4_TMRA4, TimeraIrqOverflow, Disable);
		__disable_irq();
		run_app();
		while(1);

	return 0;
}


