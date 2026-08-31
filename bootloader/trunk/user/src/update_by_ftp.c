#include <string.h>
#include <stdio.h>
#include "hc32f46_driver.h"
#include "fw_jump_helper.h"
#include "btMsg.h"

typedef enum
{
	ftpUpdateState_None = 0,
	ftpUpdateState_CheckFileHeader = 1,
	ftpUpdateState_GetBoardFw = 2,
	ftpUpdateState_GetModFw = 4,
	ftpUpdateState_FIN = 5,
} ftpUpdateStateCode;

ftpUpdateStateCode gFtpUpdateState = ftpUpdateState_CheckFileHeader;
int gPageBufIndex = 0;
int gIsUpdateBoardOnly = 0;

uint32 gFlhfilelen;
uint32 gFlhfilever;
uint32 gFlhwaddr;
int gFirstpage;
int gFullpagecnt;
int gLastpagebytecnt;
int gWritePageCnt;
int gTotDlBytes = 0;
uint8 gCrcsAndModFwDes[9];
int gBoardFwCrcsIndex = 0;

int wfaddress;
int vfaddress;
uint8 rfidCmdGetVersion[] = {0xff, 0x00, 0x03, 0x1d, 0x0c};
uint8 rfidCmdBuf[256];
int gModFwSize;
int gMod128BlkCnt = 0;
uint32 gModFwsubcrc1 = 0;
uint32 gModFwsubcrc2 = 0;
uint32 gModFwsubcrc3 = 0;
uint32 gModFwsubcrc4 = 0;

int ftp_init_callback(void)
{
	TRACE("ftp_init_callback\n");
	gFtpUpdateState = ftpUpdateState_CheckFileHeader;
	gPageBufIndex = 0;
	gTotDlBytes = 0;
	gBoardFwCrcsIndex = 0;
	gMod128BlkCnt = 0;
	return 0;
}

int VerifySlrModFw(int ckaddr, int cklen, unsigned int ckcrc)
{
	int pos = 0;
	uint16 crc;
	
	rfidCmdBuf[pos++] = 0xff;
	pos++;
	rfidCmdBuf[pos++] = 0x08;
	rfidCmdBuf[pos++] = (ckaddr >> 24) & 0xff;
	rfidCmdBuf[pos++] = (ckaddr >> 16) & 0xff;
	rfidCmdBuf[pos++] = (ckaddr >> 8) & 0xff;
	rfidCmdBuf[pos++] = (ckaddr >> 0) & 0xff;
	
	rfidCmdBuf[pos++] = (cklen >> 24) & 0xff;
	rfidCmdBuf[pos++] = (cklen >> 16) & 0xff;
	rfidCmdBuf[pos++] = (cklen >> 8) & 0xff;
	rfidCmdBuf[pos++] = (cklen >> 0) & 0xff;

	rfidCmdBuf[pos++] = (ckcrc >> 24) & 0xff;
	rfidCmdBuf[pos++] = (ckcrc >> 16) & 0xff;
	rfidCmdBuf[pos++] = (ckcrc >> 8) & 0xff;
	rfidCmdBuf[pos++] = (ckcrc >> 0) & 0xff;

	rfidCmdBuf[1] = pos-3;
	crc = rfid_calcCrc(rfidCmdBuf);
	rfidCmdBuf[pos++] = (crc >> 8) & 0xff;
	rfidCmdBuf[pos++] = (crc >> 0) & 0xff;
	write(COMMON_INTERFACE_UART0, rfidCmdBuf, pos);
	
	if (read_n(COMMON_INTERFACE_UART0, rfidCmdBuf, 7) != 7)
	{
		TRACE("VerifySlrModFw read_n(COMMON_INTERFACE_UART0, rfidCmdBuf, 7) != 7\n");
		return -3;
	}
	if (rfidCmdBuf[0] != 0xff)
	{
		TRACE("VerifySlrModFw rfidCmdBuf[0] != 0xff\n");
		return -4;
	}
	if (read_n(COMMON_INTERFACE_UART0, rfidCmdBuf+7, rfidCmdBuf[1]) != rfidCmdBuf[1])
	{
		TRACE("VerifySlrModFw read_n(COMMON_INTERFACE_UART0, rfidCmdBuf+7, rfidCmdBuf[1]) != rfidCmdBuf[1]\n");
		return -3;
	}
	if (rfidCmdBuf[3] != 0 || rfidCmdBuf[4] != 0)
	{
		TRACE("VerifySlrModFw state code err:%02X%02X\n", rfidCmdBuf[3], rfidCmdBuf[4]);
		return -7;
	}
	return 0;	
}

int WriteSlrModFlash(unsigned char wflag, 
	int waddress, unsigned char wlen, unsigned char *wdata)
{
	int pos = 0;
	uint16 crc;
	
	rfidCmdBuf[pos++] = 0xff;
	pos++;
	rfidCmdBuf[pos++] = 0x01;
	rfidCmdBuf[pos++] = wflag;
	rfidCmdBuf[pos++] = (waddress >> 24) & 0xff;
	rfidCmdBuf[pos++] = (waddress >> 16) & 0xff;
	rfidCmdBuf[pos++] = (waddress >> 8) & 0xff;
	rfidCmdBuf[pos++] = (waddress >> 0) & 0xff;
	rfidCmdBuf[pos++] = wlen/4;
	memcpy(rfidCmdBuf+pos, wdata, wlen);
	pos += wlen;
	rfidCmdBuf[1] = pos-3;
	crc = rfid_calcCrc(rfidCmdBuf);
	rfidCmdBuf[pos++] = (crc >> 8) & 0xff;
	rfidCmdBuf[pos++] = (crc >> 0) & 0xff;
	write(COMMON_INTERFACE_UART0, rfidCmdBuf, pos);
	
	if (read_n(COMMON_INTERFACE_UART0, rfidCmdBuf, 7) != 7)
	{
		TRACE("WriteSlrModFlash read_n(COMMON_INTERFACE_UART0, rfidCmdBuf, 7) != 7\n");
		return -3;
	}
	if (rfidCmdBuf[0] != 0xff)
	{
		TRACE("WriteSlrModFlash rfidCmdBuf[0] != 0xff\n");
		return -4;
	}
	if (read_n(COMMON_INTERFACE_UART0, rfidCmdBuf+7, rfidCmdBuf[1]) != rfidCmdBuf[1])
	{
		TRACE("WriteSlrModFlash read_n(COMMON_INTERFACE_UART0, rfidCmdBuf+7, rfidCmdBuf[1]) != rfidCmdBuf[1]\n");
		return -3;
	}
	if (rfidCmdBuf[3] != 0 || rfidCmdBuf[4] != 0)
	{
		TRACE("WriteSlrModFlash state code err:%02X%02X\n", rfidCmdBuf[3], rfidCmdBuf[4]);
		return -7;
	}
	return 0;
}

int ftp_data_callback(uint8 *buf, int len)
{
	brdcst_conf_handler();
	gTotDlBytes += len;
	if (gFtpUpdateState == ftpUpdateState_CheckFileHeader)
	{
		memcpy(gPageBuffer+gPageBufIndex, buf, len);
		gPageBufIndex += len;
		if (gPageBufIndex >= 18)
		{
			int pos = 0;
			if (gPageBuffer[pos] != 0xfd || gPageBuffer[pos+1] != 0x19 || 
				 gPageBuffer[pos+2] != 0x78 || gPageBuffer[pos+3] != 0x01 || 
				gPageBuffer[pos+4] != 0x26)
			{
				TRACE("first 5 bytes error:%02X %02X %02X %02X %02X\n", 
					gPageBuffer[pos], gPageBuffer[pos+1], gPageBuffer[pos+2], 
					gPageBuffer[pos+3], gPageBuffer[pos+4]);
				return -1;
			}
			pos += 5;
			if (gPageBuffer[pos] < 0x01 || gPageBuffer[pos] > 0x04)
			{
				TRACE("6th byte error:%02X\n", gPageBuffer[pos]);
				return -2;
			}
         if (gPageBuffer[pos] == 0x02 || gPageBuffer[pos] == 0x04)
				gIsUpdateBoardOnly = 1;
			else
				gIsUpdateBoardOnly = 0;
			
			pos++;
			gFlhfilelen = GetNumU32(gPageBuffer+pos);
			pos += 4;
			gFlhfilever = GetNumU32(gPageBuffer+pos);
			pos += 4;
			gFlhwaddr = GetNumU32(gPageBuffer+pos);
			pos += 4;
			
			gFirstpage = gFlhwaddr / PAGE_SIZE;
			gWritePageCnt = 0;
			gFullpagecnt = gFlhfilelen / PAGE_SIZE;
			gLastpagebytecnt = gFlhfilelen % PAGE_SIZE;
			
			memmove(gPageBuffer, gPageBuffer+pos, gPageBufIndex - pos);
			gPageBufIndex = gPageBufIndex - pos;
			gFtpUpdateState = ftpUpdateState_GetBoardFw;
		}
	}
	else if (gFtpUpdateState == ftpUpdateState_GetBoardFw)
	{
		if (gTotDlBytes > gFlhfilelen+18)
		{
			int i;
			int ncrc;
//			printf("1111111111111 get crc gTotDlBytes:%d, gFlhfilelen:%d, gBoardFwCrcsIndex:%d, len:%d\n", 
//				gTotDlBytes, gFlhfilelen, gBoardFwCrcsIndex, len);

			if (gBoardFwCrcsIndex == 0)
			{
				ncrc = gTotDlBytes-(gFlhfilelen+18);
				if (ncrc > 9)
					ncrc = 9;
				for (i = 0; i < ncrc; ++i)
					gCrcsAndModFwDes[gBoardFwCrcsIndex++] = buf[gFlhfilelen + 18 - (gTotDlBytes - len)+i];
			}
			else
			{
				ncrc = 9 - gBoardFwCrcsIndex;
				for (i = 0; i < ncrc; ++i)
					gCrcsAndModFwDes[gBoardFwCrcsIndex++] = buf[i];
			}
		}
		
		if (gPageBufIndex+len < PAGE_SIZE)
		{
			memcpy(gPageBuffer+gPageBufIndex, buf, len);
			gPageBufIndex += len;
		}
		else
		{
			int nbufspace = PAGE_SIZE-gPageBufIndex;
			memcpy(gPageBuffer+gPageBufIndex, buf, nbufspace);
			fw_revert4bytes(gPageBuffer, PAGE_SIZE);
			flash_sector_erase((gFirstpage+gWritePageCnt)*PAGE_SIZE);
			flash_bytes_write((gFirstpage+gWritePageCnt)*PAGE_SIZE, gPageBuffer, PAGE_SIZE);
			TRACE("write page %d\n", gFirstpage+gWritePageCnt);
			
			gWritePageCnt++;			
			memcpy(gPageBuffer, buf+nbufspace, len - nbufspace);
			gPageBufIndex = len - nbufspace;
		}
		
		if (gTotDlBytes >= gFlhfilelen+9+18)
		{
			int modpos;
			commonUartPara uart0Para;
			uint8 modserial;
			
			fw_revert4bytes(gPageBuffer, PAGE_SIZE);
			flash_sector_erase((gFirstpage+gWritePageCnt)*PAGE_SIZE);
			flash_bytes_write((gFirstpage+gWritePageCnt)*PAGE_SIZE, gPageBuffer, PAGE_SIZE);

			modpos = gFlhfilelen + 18 + 9 - (gTotDlBytes - len);
			memcpy(gPageBuffer, buf+modpos, len-modpos);
			gPageBufIndex = len - modpos;
			
			if (gIsUpdateBoardOnly == 1)
				gFtpUpdateState = ftpUpdateState_FIN;
			else
			{
				int uartret;
				gFtpUpdateState = ftpUpdateState_GetModFw;
				memset(&uart0Para, 0, sizeof(commonUartPara));
				uart0Para.isBlock	= O_BLOCK;
				uart0Para.isPrintf	= 0;
				uart0Para.baudrate	= 115200;
				uart0Para.timeout	= 3000;
				
				uart_close(COMMON_INTERFACE_UART0);
				uart_open(COMMON_INTERFACE_UART0, &uart0Para);
				write(COMMON_INTERFACE_UART0, rfidCmdGetVersion, sizeof(rfidCmdGetVersion));
				uartret = read_n(COMMON_INTERFACE_UART0, rfidCmdBuf, 7);
				if (uartret != 7)
				{
					if (uartret > 0)
					{
						int c;
						TRACE("err:");
						for (c = 0; c < uartret; ++c)
							TRACE("%02X ", rfidCmdBuf[c]);
						TRACE("\n");
					}
					
					TRACE("read_n(COMMON_INTERFACE_UART0, rfidCmdBuf, 7) != 7:%d\n", uartret);
					led_toggle(-1, 2000, NULL);
					while(1);
				}
				if (rfidCmdBuf[0] != 0xff)
				{
					TRACE("rfidCmdBuf[0] != 0xff\n");
					return -4;
				}
				if (read_n(COMMON_INTERFACE_UART0, rfidCmdBuf+7, rfidCmdBuf[1]) != rfidCmdBuf[1])
				{
					TRACE("read_n(COMMON_INTERFACE_UART0, rfidCmdBuf+7, rfidCmdBuf[1]) != rfidCmdBuf[1]\n");
					return -3;
				}
				else
				{
					int c;
					TRACE("resp: ");
					for (c = 0; c < rfidCmdBuf[1]+7; ++c)
						TRACE("%02X ", rfidCmdBuf[c]);
					TRACE("\n");
				}
				
				modserial = 0xf4;
				switch (rfidCmdBuf[9])
				{
					case 0xA0:
					case 0xA1:
					case 0xA9:
					case 0xA8:
					case 0xAA:
					case 0xAB:
						wfaddress = 0x00104000;
						vfaddress = 0x00104000;
						break;
					case 0xA3:
					case 0xA4:
					case 0xA6:
					case 0xA2:
					case 0xA5:
					case 0xA7:
						wfaddress = 0x08003000;
						vfaddress = 0x08003000;	
						break;
					case 0x31:
					case 0x32:
					case 0x33:
					case 0x34:
						wfaddress = 0x08008000;
						vfaddress = 0x08008000;
						modserial = 0xf6;
						break;
					default:
					{
						TRACE("invalid mod type\n");
						return -5;
					}
				}
				gModFwSize = GetNumU32(gCrcsAndModFwDes+5);
				TRACE("gCrcsAndModFwDes[4]:%d, gModFwSize:%d, modserial:%d\n", 
					gCrcsAndModFwDes[4], gModFwSize, modserial);
				if (modserial != gCrcsAndModFwDes[4])
				{
					TRACE("modserial != gCrcsAndModFwDes[4]\n");
					return -6;
				}
				TRACE("by now is ok\n");
			}
			
			TRACE("write last page %d\n", gFirstpage+gWritePageCnt);
		}
		
//		TRACE("gWritePageCnt:%d, gTotDlBytes:%d\n", gWritePageCnt, gTotDlBytes);
	}
	else if (gFtpUpdateState == ftpUpdateState_GetModFw)
	{
		int wcnt;
		int i;
		int j;
		int wret;
		uint8 wflag = 0x00;
		
		memcpy(gPageBuffer+gPageBufIndex, buf, len);
		gPageBufIndex += len;
		
		if (gPageBufIndex >= 128)
		{
			wcnt = gPageBufIndex / 128;
			for (i = 0; i < wcnt; ++i)
			{
				for (j = 0; j < 128; ++j)
				{
					if (j % 4 == 0)
						gModFwsubcrc1 += gPageBuffer[i*128+j];
					else if (j % 4 == 1)
						gModFwsubcrc2 += gPageBuffer[i*128+j];
					else if (j % 4 == 2)
						gModFwsubcrc3 += gPageBuffer[i*128+j];
					else if (j % 4 == 3)
						gModFwsubcrc4 += gPageBuffer[i*128+j];					
				}
				if (gModFwSize % 128 == 0)
				{
					if (i+gMod128BlkCnt+1 == gModFwSize / 128)
					{
						TRACE("if (i+gMod128BlkCnt+1 == gModFwSize / 128)\n");
						wflag = 0xff;
					}
				}
				wret = WriteSlrModFlash(wflag, wfaddress+(i+gMod128BlkCnt)*128, 128, gPageBuffer+i*128);
				if (wret != 0)
				{
					TRACE("0000 WriteSlrModFlash error:%d\n", wret);
					return wret;
				}

				TRACE("write mod fw %d blk ok\n", i+gMod128BlkCnt);
				
			}
			gMod128BlkCnt += wcnt;
			memmove(gPageBuffer, gPageBuffer+wcnt*128, gPageBufIndex % 128);
			gPageBufIndex = gPageBufIndex % 128;
		}
		
		if (gModFwSize % 128 != 0)
		{		
			if (gModFwSize - gMod128BlkCnt*128 == gPageBufIndex)
			{
				for (j = 0; j < gPageBufIndex; ++j)
				{
					if (j % 4 == 0)
						gModFwsubcrc1 += gPageBuffer[j];
					else if (j % 4 == 1)
						gModFwsubcrc2 += gPageBuffer[j];
					else if (j % 4 == 2)
						gModFwsubcrc3 += gPageBuffer[j];
					else if (j % 4 == 3)
						gModFwsubcrc4 += gPageBuffer[j];
				}
				wflag = 0xff;
				wret = WriteSlrModFlash(wflag, wfaddress+(gMod128BlkCnt)*128, gPageBufIndex, gPageBuffer);
				if (wret != 0)
				{
					TRACE("11111 WriteSlrModFlash error:%d\n", wret);
					return wret;
				}
//				else
//					printf("write last mod fw blk gPageBufIndex:%d ok\n", gPageBufIndex);
			}
		}
		if (wflag == 0xff)
		{
			uint32 mfcrc = (gModFwsubcrc1 & 0xff) << 24 | (gModFwsubcrc2 & 0xff) << 16 | 
				(gModFwsubcrc3 & 0xff) << 8 | (gModFwsubcrc4 & 0xff);
			wret = VerifySlrModFw(vfaddress, gModFwSize/4, mfcrc);
			if (wret != 0)
			{
				TRACE("VerifySlrModFw error:%d\n", wret);
				return wret;
			}
			else
			{
				gFtpUpdateState = ftpUpdateState_FIN;
				TRACE("VerifySlrModFw ok\n");
			}
		}
	}
//	else
//		printf("ignore no use data len:%d\n", len);
	return 0;
}

uint8 usr_ftp_buffer[600];
uint8 usr_calc_xor(uint8 *cmd, int clen)
{
	int i;
	uint8 ret = 0x00;
	
	for (i = 0; i < clen; ++i)
		ret ^= cmd[i];
	return ret;
}

void usr_send_ftp_cmd(uint8 cid, uint8 pver, uint8 *data, int dlen)
{
	int pos = 0;
	
	usr_ftp_buffer[pos++] = 0x55;
	usr_ftp_buffer[pos++] = 0xFC;
	usr_ftp_buffer[pos++] = 0xAA;
	SetNumU16(usr_ftp_buffer+pos, dlen+5);
	pos += 2;
	usr_ftp_buffer[pos++] = pver;
	usr_ftp_buffer[pos++] = cid;
	if (dlen > 0)
	{
		memcpy(usr_ftp_buffer+pos, data, dlen);
		pos += dlen;
	}
	usr_ftp_buffer[pos++] = usr_calc_xor(usr_ftp_buffer, dlen+7);
	write(COMMON_INTERFACE_UART1, usr_ftp_buffer, pos);
/*
	{
		int i;
		TRACE("ftpcmd send:");
		for (i = 0; i < pos; ++i)
			TRACE("%02X ", usr_ftp_buffer[i]);
		TRACE("\n");
	}
	*/
}
int usr_recv_ftp_cmd(uint8 cid, uint8 pver, int timeout, int *dlen)
{
	uint8 cmdxor;
	int err = 0;
	int timeout2 = 20;
	ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_SET_TIMEOUT, &timeout);
	err = read(COMMON_INTERFACE_UART1, usr_ftp_buffer, 1);
	if (err != 1)
	{
		TRACE("usr_recv_ftp_cmd if (ret != 1)\n");
		goto FIN;
	}
	ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_SET_TIMEOUT, &timeout2);
	err = read_n(COMMON_INTERFACE_UART1, usr_ftp_buffer+1, 7);
	if (err != 7)
	{
		TRACE("usr_recv_ftp_cmd if (ret != 7)\n");
		goto FIN;
	}
	if (pver != usr_ftp_buffer[5])
	{
		TRACE("usr_recv_ftp_cmd pver error\n");
		err = -3;
		goto FIN;	
	}
	
	if (cid != usr_ftp_buffer[6])
	{
		TRACE("usr_recv_ftp_cmd cid error\n");
		err = -4;
		goto FIN;	
	}
	
	if (usr_ftp_buffer[0] != 0x55 || usr_ftp_buffer[1] != 0xFC || 
		usr_ftp_buffer[2] != 0xAA)
	{
		TRACE("usr_recv_ftp_cmd header error\n");
		err = -5;
		goto FIN;
	}
	
	*dlen = GetNumU16(usr_ftp_buffer+3) - 5;
	if (*dlen  > 0)
	{
		err = read_n(COMMON_INTERFACE_UART1, usr_ftp_buffer+8, *dlen);
		if (err != *dlen)
		{
			TRACE("usr_recv_ftp_cmd recv data error\n");
			goto FIN;
		}		
	}
/*
	{
		int i;
		TRACE("ftpcmd recv:");
		for (i = 0; i < (*dlen)+8; ++i)
			TRACE("%02X ", usr_ftp_buffer[i]);
		TRACE("\n");
	}
*/
	err = 0;
	cmdxor = usr_calc_xor(usr_ftp_buffer, (*dlen)+7);
	if (cmdxor != usr_ftp_buffer[(*dlen)+7])
	{
		TRACE("usr_recv_ftp_cmd cmdxor error\n");
		err = -6;
		goto FIN;
	}
	
	if (usr_ftp_buffer[7] != 0x01)
	{
		TRACE("usr_recv_ftp_cmd statecode:%d\n", usr_ftp_buffer[7]);
		err = -7;
		goto FIN;
	}
		
FIN:
	if (err != 0)
	{
		sleep_ms(100);
		ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
	}
	return err;
}

int usr_enter_ftp_mode()
{
	int datalen;
	int ret;
	
	usr_send_ftp_cmd(0xA0, 0x01, NULL, 0);
	ret = usr_recv_ftp_cmd(0xA0, 0x01, 1000, &datalen);
	if (ret != 0)
	{
		TRACE("usr_enter_ftp_mode usr_recv_ftp_cmd error:%d\n", ret);
		return -1;
	}
	
	return 0;
}


int usr_set_ftp_addr_port(char *addr, uint16 port)
{
	char data[100];
	int ret;
	int datalen;
	
	sprintf(data, "%s:%d", addr, port);
	usr_send_ftp_cmd(0xA1, 0x01, (uint8*)data, strlen(data));
	ret = usr_recv_ftp_cmd(0xA1, 0x01, 15000, &datalen);	
	if (ret != 0)
	{
		TRACE("usr_set_ftp_addr_port usr_recv_ftp_cmd error:%d\n", ret);
		return -1;
	}
	
	return 0;
}

int usr_set_ftp_user_pwd(char *user, char *pwd)
{
	char data[100];
	int ret;
	int datalen;
	
	datalen = strlen(user)+strlen(pwd)+1;
	strcpy(data, user);
	data[strlen(user)] = 0x00;
	strcpy(data+strlen(user)+1, pwd);
	usr_send_ftp_cmd(0xA2, 0x01, (uint8*)data, datalen);
	ret = usr_recv_ftp_cmd(0xA2, 0x01, 10000, &datalen);	
	if (ret != 0)
	{
		TRACE("usr_set_ftp_user_pwd usr_recv_ftp_cmd error:%d\n", ret);
		return -1;
	}
	
	return 0;	
}

int usr_set_ftp_file_path(char *path, int *filesize)
{
	int ret;
	int datalen;
	
	usr_send_ftp_cmd(0xA3, 0x01, (uint8*)path, strlen(path));
	ret = usr_recv_ftp_cmd(0xA3, 0x01, 10000, &datalen);
	if (ret != 0)
	{
		TRACE("usr_set_ftp_file_path usr_recv_ftp_cmd error:%d\n", ret);
		return -1;
	}
	
	*filesize = GetNumU32(usr_ftp_buffer+8);
	TRACE("filesize:%d\n", *filesize);
	return 0;
}

int usr_set_ftp_get_data(uint16 psize, uint16 pnumber, int *realsize)
{
	int ret;
	int datalen;
	uint8 data[4];
	
	SetNumU16(data, psize);
	SetNumU16(data+2, pnumber);
	
	usr_send_ftp_cmd(0xA4, 0x01, data, 4);
	ret = usr_recv_ftp_cmd(0xA4, 0x01, 6000, &datalen);
	if (ret != 0)
	{
		TRACE("usr_set_ftp_get_data usr_recv_ftp_cmd error:%d\n", ret);
		return -1;
	}
	
	if (pnumber != GetNumU16(usr_ftp_buffer+10))
	{
		TRACE("usr_set_ftp_get_data  pnumber error\n");
		return -1;
	}
	
	*realsize = GetNumU16(usr_ftp_buffer+3) - 10;
//	TRACE("usr_set_ftp_get_data size:%d\n", *realsize);
	return 0;	
}
#define USR_FTP_BLOCK_SIZE 512

int usr_ftp_update_fw(char *seraddr, uint16 port, 
	char *user, char *pwd, char *path)
{
	int fwsize;
	int pnum;
	int i;
	int psize;
	int lastblkcnt;
	
	ftp_init_callback();
	if (usr_enter_ftp_mode() != 0)
		return -1;
	TRACE("usr_enter_ftp_mode ok\n");
	if (usr_set_ftp_addr_port(seraddr, 21) != 0)
		return -1;
	TRACE("usr_set_ftp_addr_port ok\n");
	if (usr_set_ftp_user_pwd(user, pwd) != 0)
		return -1;
	TRACE("usr_set_ftp_user_pwd ok\n");
	if (usr_set_ftp_file_path(path, &fwsize) != 0)
		return -1;
	TRACE("usr_set_ftp_file_path ok\n");
	
	sleep_ms(18000);
	pnum = fwsize / USR_FTP_BLOCK_SIZE;
	lastblkcnt = fwsize % USR_FTP_BLOCK_SIZE;
	if (lastblkcnt != 0)
		pnum++;
	
	for (i = 1; i <= pnum; ++i)
	{
		if (usr_set_ftp_get_data(USR_FTP_BLOCK_SIZE, i, &psize) != 0)
			return -1;

		if (lastblkcnt != 0 && i == pnum)
			psize = lastblkcnt;
		
		if (i == 1 || i == pnum)
		{
			int j;
			TRACE("ftpcmd recv:");
			for (j = 0; j < psize; ++j)
				TRACE("%02X ", usr_ftp_buffer[12+j]);
			TRACE("\n\n");			
		}

		ftp_data_callback(usr_ftp_buffer+12, psize);
	}
	
	return 0;
}

void aft_ftp_update_fw(BtParams_ST *btparams)
{
	btparams->firmwareaddr = gFlhwaddr;
	btparams->firmwaresize = gFlhfilelen;
	btparams->firmwarecrc = GetNumU32(gCrcsAndModFwDes);
	btparams->firmwarever = gFlhfilever;
	btparams->updatemode = FwUpdateMode_Default;
				
	btparams->ftpuser[0] = 0;
	btparams->ftppassword[0] = 0;
	btparams->ftpaddr[0] = 0;
	btparams->filename[0] = 0;		
	dumpBtParams(btparams);
				
	if (verifyFirmware(btparams->firmwareaddr, 
		btparams->firmwaresize, btparams->firmwarecrc) != 0)
	{
		TRACE("verifyFirmware error\n");
		led_toggle(600*1000, 1200, NULL);				
		system_reset();
	}
				
	btparams->updateflag = 0;
	setBtParams(btparams);
				
	sleep_ms(200);
	system_reset();	
}
