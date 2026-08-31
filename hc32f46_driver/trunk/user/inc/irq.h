
#ifndef __irq_H
#define __irq_H
#include "types.h"
#include "hc32_ddl.h"


void TimeraUnit4_IrqCallback(void);
void Dma1ch0tcIrqCallback(void);
void Dma2ch0tcIrqCallback(void);

void Usart3RxIrqCallback(void);
void Usart2RxIrqCallback(void);
void Usart4RxIrqCallback(void);

void Usart1recErrIrqCallback_dma(void);
void Usart2recErrIrqCallback(void);
void Usart3recErrIrqCallback(void);
void Usart4recErrIrqCallback(void);

void Usart2recErrIrqCallback_dma(void);
void Usart3recErrIrqCallback_dma(void);

extern volatile uint32 gSysTickCnt;

#endif 
