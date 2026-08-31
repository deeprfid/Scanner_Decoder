#ifndef _CRC_H_
#define _CRC_H_
#include "type.h"


//#define POLY5 0x48
//#define POLY16 0x1021
//#define POLY32 0x4C11DB7
//#define CRC_SEED   0xFFFF
//#define CRC_RESULT 0x1D0F

uint16 crc_CalEPCcrc16(unsigned char *buf,unsigned short bitslength);
uint8 crc_CalEPCcrc5(unsigned char *buf,unsigned short bitslength);

void crc_b_VerifyCalEPCcrc16(unsigned short *shift,unsigned char in,short bitcnt);
void crc_b8_VerifyCalEPCcrc16(unsigned short *shift,unsigned char in);

uint16 crc_CalcCRC(uint8 *msgbuf,uint8 msglen);
#endif
