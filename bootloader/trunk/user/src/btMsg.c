#include "btMsg.h"
#include <stdio.h>
#include <string.h>
#include "bt_common.h"
#include "hc32f46_driver.h"
#include "app_conf.h"
#define PREAMBLE 0xff

#define MSG_CRC_INIT		    0xFFFF
#define MSG_CCITT_CRC_POLY		0x1021

void CRC_calcCrc8(unsigned short *crcReg, unsigned short poly, unsigned short u8Data)
{
	unsigned short i;
	unsigned short xorFlag;
	unsigned short bit;
	unsigned short dcdBitMask = 0x80;
	
	for(i=0; i<8; i++)
	{
		// Get the carry bit.  This determines if the polynomial should be xor'd
		//	with the CRC register.
		xorFlag = *crcReg & 0x8000;
		
		// Shift the bits over by one.
		*crcReg <<= 1;
		
		// Shift in the next bit in the data byte
		bit = ((u8Data & dcdBitMask) == dcdBitMask);
		*crcReg |= bit;
		
		// XOR the polynomial
		if(xorFlag)
		{
			*crcReg = *crcReg ^ poly;
		}										 
		
		// Shift over the dcd mask
		dcdBitMask >>= 1;	
	}

}

int MSG_checkCRCFromCmd(PBtMsgSt hMsg)
{
	unsigned short calcCrc = MSG_CRC_INIT;
	int  i;

	unsigned char datalen[2];
	datalen[0] = (hMsg->Datalen >> 8) & 0xff;
	datalen[1] = (hMsg->Datalen >> 0) & 0xff;

	CRC_calcCrc8(&calcCrc, MSG_CCITT_CRC_POLY, datalen[0]);
	CRC_calcCrc8(&calcCrc, MSG_CCITT_CRC_POLY, datalen[1]);
	CRC_calcCrc8(&calcCrc, MSG_CCITT_CRC_POLY, hMsg->MsgCode);
//	printf("------------------- hMsg->Datalen: %d\n", hMsg->Datalen);
	for(i=0; i<hMsg->Datalen; i++)
	{

		CRC_calcCrc8(&calcCrc, MSG_CCITT_CRC_POLY, hMsg->data[i]);
	}

	if (hMsg->CRC[0] == ((calcCrc >> 8) & 0xff ) 
		&& hMsg->CRC[1] == ((calcCrc >> 0) & 0xff))
		return 0;
	else
		return -1;	
}
unsigned short rfid_calcCrc(unsigned char *pMsg)
{
	int i;
	unsigned short calcCrc = MSG_CRC_INIT;
	for (i = 1; i < pMsg[1]+3; ++i)
		CRC_calcCrc8(&calcCrc, MSG_CCITT_CRC_POLY, pMsg[i]);
	return calcCrc;
}

void MSG_calcCrcFromDsp(PBtMsgSt hMsg)
{
	unsigned short calcCrc = MSG_CRC_INIT;
	unsigned char  i;
	unsigned char datalen[2];
	datalen[0] = (hMsg->Datalen >> 8) & 0xff;
	datalen[1] = (hMsg->Datalen >> 0) & 0xff;

	CRC_calcCrc8(&calcCrc, MSG_CCITT_CRC_POLY, datalen[0]);
	CRC_calcCrc8(&calcCrc, MSG_CCITT_CRC_POLY, datalen[1]);
	CRC_calcCrc8(&calcCrc, MSG_CCITT_CRC_POLY, hMsg->MsgCode);
	CRC_calcCrc8(&calcCrc, MSG_CCITT_CRC_POLY, hMsg->StatusCode[0]);
	CRC_calcCrc8(&calcCrc, MSG_CCITT_CRC_POLY, hMsg->StatusCode[1]);
	

	
	for(i=0; i<hMsg->Datalen; i++)
	{		
		CRC_calcCrc8(&calcCrc, MSG_CCITT_CRC_POLY, hMsg->data[i]);
	}
	hMsg->CRC[0] = (calcCrc >> 8) & 0xff;
	hMsg->CRC[1] = (calcCrc >> 0) & 0xff;

}

unsigned char sendbuf[30];
int gSocks[2];
int gTcpport;
int gStatusFlags[2];
int gStream;

int SendRespMsg(PBtMsgSt Msg)
{	
	sendbuf[0] = 0xff;
	sendbuf[1] = (Msg->Datalen >> 8) & 0xff;
	sendbuf[2] = (Msg->Datalen >> 0) & 0xff;
	sendbuf[3] = Msg->MsgCode;
	sendbuf[4] = Msg->StatusCode[0];
	sendbuf[5] = Msg->StatusCode[1];
	//if((Msg->Datalen)>22)
	//  UART0_SENT('A'); //TEST************************************************************************************
	memcpy(sendbuf+6, Msg->data, Msg->Datalen);
	MSG_calcCrcFromDsp(Msg);
	memcpy(sendbuf+6+Msg->Datalen, Msg->CRC, 2);
	write(gStream, sendbuf, Msg->Datalen+8);
	
	return 0;
}

extern Spi_Ex_Code gSpiex;
extern Uart_Ex_Code gUartex;

int RecvMsg(PBtMsgSt Msg)
{
	unsigned char buf[300];
	int nread;
	int uarts[3];
	int uartscnt = 0;
	

#if (AppDubugPrintf != 1)
	uarts[uartscnt++] = COMMON_INTERFACE_UART2;
#endif
	if (gUartex == Uart_Ex_Wlan || gUartex == Uart_Ex_Bluetooth)
		uarts[uartscnt++] = COMMON_INTERFACE_UART1;

	uarts[uartscnt++] = COMMON_INTERFACE_UART3;
	
#ifdef BTMSG_DEBUG
	int i;
#endif

	while (1)
	{
		if (gSpiex == Spi_Ex_Ethernet)
		{
			gStream = apt_pair_select_nob(gSocks, gTcpport, gStatusFlags);
			if (gStream >= 0)
				break;
		}
		
		gStream = apt_uart_select_nob(uarts, uartscnt);
		if (gStream > 0)
			break;

		gStream = apt_usb_select_nob();
		if (gStream > 0)
			break;
		if (gSpiex == Spi_Ex_Ethernet)
			brdcst_conf_handler();
		sleep_ms(15);
	}

	nread = read(gStream, buf, 1);
	if (nread != 1)
	{
		TRACE("read 0xff error \n");
		return -1;
	}
	
	if (buf[0] != 0xff)
	{
		TRACE("error PREAMBLE buf[0]:%02X\n", buf[0]);
		return -1;
	}

	nread = read_n(gStream, buf, 3);
	if (nread != 3)
	{
		TRACE("data len and cmdid error \n");
		return -1;
	}

	Msg->Datalen = (buf[0] << 8) | buf[1];
	Msg->MsgCode = buf[2];
//	printf("Msg->Datalen:%d Msg->MsgCode:%d\n", Msg->Datalen, Msg->MsgCode);
	
	nread = read_n(gStream, buf, Msg->Datalen+2);
	if (nread != Msg->Datalen+2)
	{
		TRACE("data and crc error :%d\n", nread);
		return -1;
	}

	memcpy(Msg->data, buf, Msg->Datalen);
	Msg->CRC[0] = buf[nread-2];
	Msg->CRC[1] = buf[nread-1];

	if (MSG_checkCRCFromCmd(Msg) < 0)
	{
		/*
		int i;
		printf("error msg crc dump start:\n");
		for (i = 0; i < Msg->Datalen+2; ++i)
			printf("%02X ", buf[i]);
		printf("\n");
		*/
		return -1;
	}

	return 0;
}
