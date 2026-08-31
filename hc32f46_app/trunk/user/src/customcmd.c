#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "hc32f46_driver.h"
#include "TagBuffer.h"

extern volatile int baudratenow;
void sendforall(unsigned char *buf, int buflen);
volatile extern int gCurInfFd;
READER_ERR getModlueParams(int hreader, ReaderStaticSettings_ST *pRsSettings);
extern int gRdrHandlePassive;
extern ReaderStaticSettings_ST *gPRdrStaSet;

int is_custom_cmd(int fd, uint8 *buf, int *nparse)
{
	if (buf[0] == 0xff)
	{
		if (buf[2] == 0x00)
		{
			if (read_n(fd, buf+3, 3) != 3)
				return -1;
			else
			{
				*nparse = 6;
				if (GetNumU16(buf+4) < 6)
					return 0;
				else
					return 1;
			}
		}
		else
			return 0;
	}
	else
		return 1;
}

int SaveCurStaticSettings(void)
{
	int ecode;
	ecode = getModlueParams(gRdrHandlePassive, gPRdrStaSet);
	if (ecode == 0)
		set_rdr_static_settings(gPRdrStaSet);
	return ecode;
}

/*
int custom_setactparams(unsigned char *buf)
{
	unsigned char *pData;
	int datalen;
	int ret;
	unsigned char setrespok[] = {0xff, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00};
	unsigned char setresperr[] = {0xff, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x07};
	
	datalen = (buf[2] << 8) | buf[3];
	if (datalen == 0)
	{
		erase_multi_config(0, 1);
		sendforall(setrespok, sizeof(setrespok));
		sleep_ms(1000);
		system_reset();
		return 0;
	}
	
	pData = malloc_hexp(datalen);
	ret = read_n(gCurInfFd, pData, datalen);
	if (ret <= 0)
	{
		free_hexp(pData);
		return -1;
	}
	if (buf[4] != 21)
	{
		free_hexp(pData);
		return 0;
	}

	if (SetFlashConfig(pData, datalen, &gNetConf) < 0)
	{
		sendforall(setresperr, sizeof(setresperr));
		free_hexp(pData);
		return 0;
	}

	sendforall(setrespok, sizeof(setrespok));
	sleep_ms(1000);
	system_reset();
	return 0;
}
*/
void custom_getip(unsigned char *buf)
{
	int pos = 0;

	buf[pos++] = 'I';
	buf[pos++] = 'P';
	buf[pos++] = 'G';
	buf[pos++] = 'E';
	buf[pos++] = 'T';
	
	buf[pos++] = gNetConf.ip[0];
	buf[pos++] = gNetConf.ip[1];
	buf[pos++] = gNetConf.ip[2];
	buf[pos++] = gNetConf.ip[3];
	
	buf[pos++] = gNetConf.subnetMask[0];
	buf[pos++] = gNetConf.subnetMask[1];
	buf[pos++] = gNetConf.subnetMask[2];
	buf[pos++] = gNetConf.subnetMask[3];
	
	buf[pos++] = gNetConf.gatewayIP[0];
	buf[pos++] = gNetConf.gatewayIP[1];
	buf[pos++] = gNetConf.gatewayIP[2];
	buf[pos++] = gNetConf.gatewayIP[3];
	
	buf[pos++] = gNetConf.mac[0];
	buf[pos++] = gNetConf.mac[1];
	buf[pos++] = gNetConf.mac[2];
	buf[pos++] = gNetConf.mac[3];
	buf[pos++] = gNetConf.mac[4];
	buf[pos++] = gNetConf.mac[5];
	
	buf[pos++] = (gNetConf.listenPort >> 8) & 0xff;
	buf[pos++] = (gNetConf.listenPort >> 0) & 0xff;
	sendforall(buf, pos);
}

#define SilionMACBase1 0x08
#define SilionMACBase2 0x26
#define SilionMACBase3 0xAE
#define SilionMACBase4 0x10

void custom_setip(unsigned char *buf)
{
	int pos = 5;
	
	gNetConf.ip[0] = buf[pos++];
	gNetConf.ip[1] = buf[pos++];
	gNetConf.ip[2] = buf[pos++];
	gNetConf.ip[3] = buf[pos++];
			
	gNetConf.subnetMask[0] = buf[pos++];
	gNetConf.subnetMask[1] = buf[pos++];
	gNetConf.subnetMask[2] = buf[pos++];
	gNetConf.subnetMask[3] = buf[pos++];
			
	gNetConf.gatewayIP[0] = buf[pos++];
	gNetConf.gatewayIP[1] = buf[pos++];
	gNetConf.gatewayIP[2] = buf[pos++];
	gNetConf.gatewayIP[3] = buf[pos++];
	
	if (gNetConf.mac[0] == SilionMACBase1 && 
		gNetConf.mac[1] == SilionMACBase2 && 
		gNetConf.mac[2] == SilionMACBase3 && 
		(gNetConf.mac[3] & 0xF0) == SilionMACBase4)
		pos += 6;
	else
	{
		gNetConf.mac[0] = buf[pos++];
		gNetConf.mac[1] = buf[pos++];
		gNetConf.mac[2] = buf[pos++];
		gNetConf.mac[3] = buf[pos++];
		gNetConf.mac[4] = buf[pos++];
		gNetConf.mac[5] = buf[pos++];
	}
	
	gNetConf.listenPort = (buf[pos] << 8) | buf[pos+1];
	set_network_config(&gNetConf);
	system_reset();
}

void custom_gpiget(unsigned char *buf)
{
	unsigned char state = 0;
	int pos = 0;
	state |= gpi_get(1) << 0; 
	state |= gpi_get(2) << 1;
	state |= gpi_get(3) << 2;
	state |= gpi_get(4) << 3;
	
	buf[pos++] = 'I';
	buf[pos++] = 'O';
	buf[pos++] = 'G';
	buf[pos++] = 'E';
	buf[pos++] = 'T';
	buf[pos++] = state;
	sendforall(buf, pos);
}

#define MSG_CRC_INIT		    0xFFFF
#define MSG_CCITT_CRC_POLY		0x1021

static void CRC_calcCrc8(unsigned short *crcReg, 
	unsigned short poly, unsigned short u8Data)
{
	unsigned char i;
	unsigned short xorFlag;
	unsigned short bit;
	unsigned short dcdBitMask = 0x80;
	
	for(i=0; i<8; i++)
	{

		xorFlag = *crcReg & 0x8000;
		
		*crcReg <<= 1;
		
		bit = ((u8Data & dcdBitMask) == dcdBitMask);
		*crcReg |= bit;
		
		if(xorFlag)
		{
			*crcReg = *crcReg ^ poly;
		}
		
		dcdBitMask >>= 1;	
	}
}

//msgbuf:消息体
//msglen:消息长度，不包括crc
//返回指：crc
unsigned short CalcCRC(unsigned char *msgbuf, int msglen)
{
	unsigned short calcCrc = MSG_CRC_INIT;
	unsigned short  k;

	for (k = 1; k < msglen; ++k)
     {
		CRC_calcCrc8(&calcCrc, MSG_CCITT_CRC_POLY, msgbuf[k]);
     }
	return calcCrc;
}

unsigned short ipcCrc(unsigned char *msgbuf, int msglen)
{
	unsigned short calcCrc = MSG_CRC_INIT;
	unsigned short  k;

	for (k = 0; k < msglen; ++k)
     {
		CRC_calcCrc8(&calcCrc, MSG_CCITT_CRC_POLY, msgbuf[k]);
     }
	return calcCrc;
}

void custom_gpiget2(unsigned char *buf)
{
	unsigned char state = 0;
	int pos = 0;
	unsigned short crc;
	state |= gpi_get(1) << 0;
	state |= gpi_get(2) << 1;
	state |= gpi_get(3) << 2;
	state |= gpi_get(4) << 3;
	
	buf[pos++] = 0xFF;
	buf[pos++] = 0x04;
	buf[pos++] = 0xAA;
	buf[pos++] = 0x00;
	buf[pos++] = 0x00;
	buf[pos++] = 'G';
	buf[pos++] = 'I';
	buf[pos++] = 'O';
	buf[pos++] = state;
	crc = CalcCRC(buf, pos);
	buf[pos++] = (crc >> 8) & 0xff;
	buf[pos++] = (crc >> 0) & 0xff;
	sendforall(buf, pos);
}
//extern ReaderStaticSettings_ST *gRsSetting;
void custom_gposet(unsigned char *buf)
{
	int i = 0;
	int pos = 6;
	for (i = 0; i < buf[5]; ++i)
	{
//		buf[pos+1] = 1 - buf[pos+1]; //revert state
		if (buf[pos] == 1)
		{
			gpo_set(1, buf[pos+1]);
			gPRdrStaSet->gpos[0] = buf[pos+1]+1;
		}
		else if (buf[pos] == 2)
		{
			gpo_set(2, buf[pos+1]);
			gPRdrStaSet->gpos[1] = buf[pos+1]+1;
		}
		else if (buf[pos] == 3)
		{
			gpo_set(3, buf[pos+1]);
			gPRdrStaSet->gpos[2] = buf[pos+1]+1;
		}
		else if (buf[pos] == 4)
		{
			gpo_set(4, buf[pos+1]);
			gPRdrStaSet->gpos[3] = buf[pos+1]+1;
		}
		else if (buf[pos] == 5)
			gpo_set(5, buf[pos+1]);
		
		pos += 2;
	}
	pos = 0;
	buf[pos++] = 'I';
	buf[pos++] = 'O';
	buf[pos++] = 'S';
	buf[pos++] = 'E';
	buf[pos++] = 'T';
	buf[pos++] = 'O';
	buf[pos++] = 'K';
	sendforall(buf, pos);
}
void custom_gposet2(unsigned char *buf)
{
	int i = 0;
	int pos = 4;
	unsigned short crc;
	for (i = 0; i < buf[3]; ++i)
	{
		if (buf[pos] == 1)
		{
			gpo_set(1, buf[pos+1]);
			gPRdrStaSet->gpos[0] = buf[pos+1]+1;
		}
		else if (buf[pos] == 2)
		{
			gpo_set(2, buf[pos+1]);
			gPRdrStaSet->gpos[1] = buf[pos+1]+1;
		}
		else if (buf[pos] == 3)
		{
			gpo_set(3, buf[pos+1]);
			gPRdrStaSet->gpos[2] = buf[pos+1]+1;
		}
		else if (buf[pos] == 4)
		{
			gpo_set(4, buf[pos+1]);
			gPRdrStaSet->gpos[3] = buf[pos+1]+1;
		}
		else if (buf[pos] == 5)
			gpo_set(5, buf[pos+1]);		

		pos += 2;
	}
	pos = 0;
	buf[pos++] = 0xFF;
	buf[pos++] = 0x03;
	buf[pos++] = 0xAA;
	buf[pos++] = 0x00;
	buf[pos++] = 0x00;
	buf[pos++] = 'S';
	buf[pos++] = 'I';
	buf[pos++] = 'O';
	crc = CalcCRC(buf, pos);
	buf[pos++] = (crc >> 8) & 0xff;
	buf[pos++] = (crc >> 0) & 0xff;
	sendforall(buf, pos);
}
void custom_resetmodule(int uart1fd, unsigned char *buf)
{
	int resetflag = 1;
	int i;
	for (i = 0; i < 32; ++i)
	{
		if (buf[i] != 0xFF)
		{
			resetflag = 0;
			break;
		}
	}
	if (resetflag == 1)
		system_reset();
}

void custom_setm6ebaud230400(int uartfd, unsigned char *buf)
{
	int baudrate = 230400;
	unsigned char cmdset230k[] = {0xff, 0x04, 0x06, 0x00, 0x03, 0x84, 0x00, 0xC2, 0x22};
	if (baudratenow != 230400)
	{
		write(uartfd, cmdset230k, sizeof(cmdset230k));
//	read_n(uart1fd, recvbuffer, 7);
		sleep_ms(350);
		ioctl(uartfd, COMMON_INTERFACE_SET_BAUDRATE, &baudrate);
	}
	buf[0] = 'O';
	buf[1] = 'K';
	sendforall(buf, 2);
}

void custom_getconfig(unsigned char *buf)
{
	unsigned char addr = buf[8];
	int pos = 0;

	buf[pos++] = 'C';
	buf[pos++] = 'O';
	buf[pos++] = 'N';
	buf[pos++] = 'F';
	buf[pos++] = 'I';
	buf[pos++] = 'G';
	buf[pos++] = 'R';
	buf[pos++] = 'X';
	buf[pos++] = 0;
	buf[pos++] = 0;
	buf[pos++] = addr;
	get_passivemode_config((addr-0xA0)*200, buf+pos, 200);
	pos += 200;
	sendforall(buf, pos);
}

void custom_setconfig(unsigned char *buf)
{
	unsigned char addr = buf[8];
	int pos = 0;
	TRACE("custom_setconfig\n");
	set_passivemode_config((addr-0xA0)*200, buf+9, 200);
	buf[pos++] = 'C';
	buf[pos++] = 'O';
	buf[pos++] = 'N';
	buf[pos++] = 'F';
	buf[pos++] = 'I';
	buf[pos++] = 'G';
	buf[pos++] = 'W';
	buf[pos++] = 'X';
	buf[pos++] = 0;
	buf[pos++] = 0;
	buf[pos++] = addr;
	sendforall(buf, pos);
}


