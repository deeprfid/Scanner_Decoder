#ifndef _sysinit_H_
#define _sysinit_H_

#include "hc32_ddl.h"
#include "stdio.h"
#include "string.h"
#include "flash.h"





void sysinit(void);
void sysinitRCC_Configuration(void);
void sysinitGPIO_Configuration(void);


#endif
