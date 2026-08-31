
#ifndef _COMMON_H
#define _COMMON_H

#include "stdio.h"
#include "string.h"
#include "flash.h"
#include "hc32_ddl.h"
#include "crc.h"

unsigned char Write_Page(unsigned int Page_Number,unsigned int wordaddr,unsigned int *Buffer,unsigned int len,unsigned char pagestart,unsigned char pageend);
void Read_Page(unsigned int readaddr,unsigned int *Buffer,unsigned int len);
void CmdRespone(uint8_t cmdval,uint16_t resstate,uint8_t datlen,uint8_t *datval);
void RCC_Configuration(void);
void GPIO_Configuration(void);
void USART3_Config(uint32_t u32Baudrate);
void IAP_ResetConfig(void);
void run_app(void);
void USART3_IT_DISABLE(void);
void USART3_IT_ENABLE(void);
void CRC_calcCrc8(uint16_t *crcReg, uint16_t poly, uint16_t u8Data);
unsigned short CalcCRC(uint8_t *msgbuf,uint8_t msglen);
void time_ini(void);

 
#endif 

