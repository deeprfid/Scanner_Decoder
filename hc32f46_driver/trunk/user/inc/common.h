
#ifndef _COMMON_H
#define _COMMON_H

#include <stdio.h>
#include <string.h>


void RCC_Configuration(void);
void GPIO_Configuration(void);
void time_ini(void);
void USART3_IT_DISABLE(void);
void USART3_IT_ENABLE(void);


#define E(x)   \
	err = x; \
	if (err != 0) \
{ \
	TRACE("err at %s\n", #x); \
		goto FIN; \
}  \
	\
	


 
#endif 

