#include <stdio.h>
#include <stdlib.h>
#include "arm7drive.h"
#include "ModuleReader.h"
#include "TagBuffer.h"
#include "ErrChecker.h"

int firmware_information_get(int fd)
{
	return 0;
}

int user_main_thstk_size = 1024*4;
int user_main_priority = 10;
int is_enable_fwupdate = 0;
int heap_base_address = 0x00209620;
int gRS232OR485 = 0;
int gPrintfInterface = 0;
int gEmacSpeed = 0;

int tcp9999_putchar(int ch)
{
	return ch;
}
int reason2 = 0;
int reason1 = 0;

READER_ERR gRdrErr = MT_OK_ERR;
volatile int gErrSend = 0;
volatile int gRdrStateFlag = 0;


int gIsCaliR2000Reg = 0;
int hreader;
volatile int gIsFinInit = 0;
volatile int gUart0fd = 0;
volatile int gIsUartSend = 0;
struct sockaddr_in serv_addr;
void CheckServerConnection(struct sockaddr_in *serip);
int Write_N_NoBlk(int fd, unsigned char *buf, int len)
{
	int pos = 0;
	int nleft = len;
	int nret = 0;
	unsigned char *pbuf = buf;
	while (nleft > 0)
	{
		nret = write(fd, pbuf+pos, nleft);
		if (nret  < 0)
		{
			printf("write pos:%d, nret:%d\n", pos, nret);	
//			if ((errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
//			{
//				printf("(errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)\n");
//				continue;
//			}
//			else
				printf("before close\n");
				close(fd);
				printf("after close\n");
				CheckServerConnection(&serv_addr);
				return 1;
		}
		else if (nret == 0)
		{
			printf("1111 close fd by server reconnect....\n");
			close(fd);
			os_dly_wait(200);
			CheckServerConnection(&serv_addr);
			return 1;
		}
		else
		{
			pos += nret;
			nleft -= nret;
		}
	}
	return 0;
}

typedef struct
{
	unsigned char main_board;
	unsigned char rfid_mod;
	unsigned char software_version[4];
	unsigned char antcount;
	unsigned char connected_antennas[16];
	int hb_count;
} HeartBeatData_ST;

HeartBeatData_ST gHbData;

AppCustomParams *gAppCusParam;
volatile int fdconnect;
unsigned long long glastpulsetm;
unsigned long long gnowpulsetm;

unsigned long long glastsendtagstm;

unsigned long long gInitGetTemperTime;
unsigned long long gLastGetTemperTime;
int gLastRfidTemperatrue;

void CheckServerConnection(struct sockaddr_in *serip)
{
	int m_wtm = 4000;
	int m_rtm = 20;
	int nagle = 1;
	long mode  = 1;
	int waittime;
	printf("enter CheckServerConnection\n");
	
	while (1)
	{
		if ((fdconnect = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		{
			printf("connect server reset by create socket\n");
			system_reset();
		}
		if (connect(fdconnect, (struct sockaddr *)serip, sizeof(struct sockaddr)) != 0) 
		{
			close(fdconnect);
			os_dly_wait(200);
			continue;
		}
		else
		{
			printf("connect server successfully\n");
			
			if (setsockopt(fdconnect, IPPROTO_TCP, TCP_NODELAY, &nagle, sizeof(nagle)) != 0)
			{
				close(fdconnect);
				printf("connect server reset by set TCP_NODELAY\n");
				system_reset();
			}
			
			if (setsockopt(fdconnect, SOL_SOCKET, SO_SNDTIMEO, &m_wtm, sizeof(m_wtm)) != 0)
			{
				close(fdconnect);
				printf("connect server reset by set SO_SNDTIMEO\n");
				system_reset();
			}
			/*
			if (setsockopt(fdconnect, SOL_SOCKET, SO_RCVTIMEO, &m_rtm, sizeof(m_rtm)) != 0)
			{
				close(fdconnect);
				printf("connect server reset by set SO_RCVTIMEO\n");
				system_reset();
			}*/
/*
			if (ioctl(fdconnect, FIONBIO , &mode) != 0)
			{
				close(fdconnect);
				printf("connect server reset by ioctl\n");
				system_reset();
			}*/
			waittime = ((unsigned int)rand()) % 500;
			if (waittime < 5)
				waittime = 10;
			os_dly_wait(waittime);
			break;
		}
	}
}

ConnAnts_ST gConnants;
int gAntNumber = -1;
int gFistdetAntCnt = -1;
READER_ERR OpenReader(int *prdr)
{
	unsigned char rdrver[8] = {0};
	Reader_Type rdrType;
	int tmpint;
	unsigned short tmpushort;
	AntPowerConf pwrs;
	int i;
	READER_ERR rerr = MT_OK_ERR;
	printf("power off module\n");
	io_negate_write(GPIO_MODULE_POWER_ON, 0);
	os_dly_wait(200);
	printf("power on module\n");
	io_negate_write(GPIO_MODULE_POWER_ON, 1);
	os_dly_wait(100);

	if (gAntNumber == 4)
		rdrType = MODULE_FOUR_ANTS;
	else
		rdrType = MODULE_ONE_ANT;
	
	if (gAntNumber == -1)
	{
		rerr = InitReader(&hreader, "uart1", MODULE_ONE_ANT);
		if (rerr != MT_OK_ERR)
		{
			printf("InitReader err:%d\n", rerr);
			return rerr;
		}
		rerr = ParamGet(hreader, MTR_PARAM_READER_VERSION, rdrver);
		if (rerr != MT_OK_ERR)
		{
			CloseReader(hreader);
			printf("MTR_PARAM_READER_VERSION err:%d\n", rerr);
			return rerr;
		}
		if (rdrver[0] == 0xA0)
		{
			printf("find 1100 module\n");
			gAntNumber = 4;
			rdrType = MODULE_FOUR_ANTS;
			gHbData.rfid_mod = MODOULE_SLR1100;
		}
		else
		{
			printf("find 5100 module\n");
			gAntNumber = 1;
			rdrType = MODULE_ONE_ANT;
			if (rdrver[0] == 0xA1)
				gHbData.rfid_mod = MODOULE_SLR1200;
			else if (rdrver[0] == 0xA3)
				gHbData.rfid_mod = MODOULE_SLR5100;
			else if (rdrver[0] == 0xA4)
				gHbData.rfid_mod = MODOULE_SLR5300;
			else
				gHbData.rfid_mod = MODOULE_NONE;
		}
		CloseReader(hreader);
	}

	printf("before InitReader\n");
	rerr = InitReader(prdr, "uart1", rdrType);
	if (rerr != MT_OK_ERR)
		return rerr;
	printf("InitReader ok\n");
	
	if (gAntNumber != 1)
	{
		rerr = ParamGet(*prdr, MTR_PARAM_READER_CONN_ANTS, &gConnants);
		printf("after ParamGet MTR_PARAM_READER_CONN_ANTS:%d, antcnt:%d\n", rerr, gConnants.antcnt);
		if (rerr != MT_OK_ERR)
		{
			CloseReader(*prdr);
			return rerr;
		}
		else
		{
			printf("gConnants.antcnt:%d\n", gConnants.antcnt);
			if (gConnants.antcnt == 0)
			{
				printf("MT_HARDWARE_ALERT_ERR_BY_NO_ANTENNAS\n");
				CloseReader(*prdr);
				return MT_HARDWARE_ALERT_ERR_BY_NO_ANTENNAS;
			}
			else
				gHbData.antcount = gConnants.antcnt;
			for (i = 0; i < gHbData.antcount; ++i)
				gHbData.connected_antennas[i] = gConnants.connectedants[i];
			
			if (gFistdetAntCnt == -1)
				gFistdetAntCnt = gConnants.antcnt;
			else
			{
				if (gConnants.antcnt < gFistdetAntCnt)
				{
					printf("some ant lost MT_HARDWARE_ALERT_ERR_BY_NO_ANTENNAS\n");
					CloseReader(*prdr);
					return MT_HARDWARE_ALERT_ERR_BY_NO_ANTENNAS;
				}
			}			
		}		
	}
	else
	{
		gHbData.antcount = 0;
		gConnants.antcnt = 1;
		gConnants.connectedants[0] = 1;
	}

	
	tmpint = gAppCusParam->gen2session;
	rerr = ParamSet(*prdr, MTR_PARAM_POTL_GEN2_SESSION, &tmpint);
	if (rerr != MT_OK_ERR)
	{
		CloseReader(*prdr);
		return rerr;
	}
	printf("ParamSet MTR_PARAM_POTL_GEN2_SESSION ok\n");
	
	if (gAppCusParam->rpwrs_len == 0)
	{
		printf("if (gAppCusParam.rpwrs_len == 0)\n");
		rerr = ParamGet(*prdr, MTR_PARAM_RF_MAXPOWER, &tmpushort);
		if (rerr != MT_OK_ERR)
		{
			CloseReader(*prdr);
			return rerr;
		}
		pwrs.antcnt = gAntNumber;
		for (i = 0; i < gAntNumber; ++i)
		{
			pwrs.Powers[i].antid = i+1;
			pwrs.Powers[i].readPower = tmpushort;
			pwrs.Powers[i].writePower = tmpushort;
		}
	}
	else
	{
		if (gAppCusParam->rpwrs[0] == 0)
		{
			printf("gAppCusParam.rpwrs[1]:%d, gAntNumber:%d\n", gAppCusParam->rpwrs[1],gAntNumber);
			pwrs.antcnt = gAntNumber;
			for (i = 0; i < gAntNumber; ++i)
			{
				pwrs.Powers[i].antid = i+1;
				pwrs.Powers[i].readPower = gAppCusParam->rpwrs[1];
				pwrs.Powers[i].writePower = gAppCusParam->rpwrs[1];
			}		
		}
		else
		{
			printf("if (gAppCusParam.rpwrs[0] != 0)\n");
			pwrs.antcnt = gAntNumber;
			for (i = 0; i < gAntNumber; ++i)
			{
				pwrs.Powers[i].antid = i+1;
				pwrs.Powers[i].readPower = gAppCusParam->rpwrs[0];
				pwrs.Powers[i].writePower = gAppCusParam->rpwrs[0];
			}	
		}
	}
	rerr = ParamSet(*prdr, MTR_PARAM_RF_ANTPOWER, &pwrs);
	if (rerr != MT_OK_ERR)
	{
		printf("ParamSet MTR_PARAM_RF_ANTPOWER failed:%d\n", rerr);
		CloseReader(*prdr);
		return rerr;
	}
	printf("ParamSet MTR_PARAM_RF_ANTPOWER ok\n");
	
	tmpint = gAppCusParam->region;
	rerr = ParamSet(*prdr, MTR_PARAM_FREQUENCY_REGION, &tmpint);
	if (rerr != MT_OK_ERR)
	{
		CloseReader(*prdr);
		return rerr;
	}
	printf("ParamSet MTR_PARAM_FREQUENCY_REGION ok\n");
	if (gIsCaliR2000Reg == 1 && gAntNumber != 1)
	{
		CustomParam_ST cusParam;
		unsigned char cusBytes[10];		
		unsigned int regval;
		
		cusParam.pCusParam = cusBytes;
		cusParam.CParamlen = 10;
		cusBytes[0] = 0x03;
		cusBytes[1] = (0X115 >> 24) & 0xff;
		cusBytes[2] = (0X115 >> 16) & 0xff;
		cusBytes[3] = (0X115 >> 8) & 0xff;
		cusBytes[4] = (0X115 >> 0) & 0xff;
		rerr = ParamGet(*prdr, MTR_PARAM_CUSTOM, &cusParam);
		if (rerr != MT_OK_ERR)
		{
			printf("get 0x115 failed\n");
			CloseReader(*prdr);
			return rerr;
		}
		regval = cusBytes[3] | 0x100;
		cusBytes[0] = 0x03;
		cusBytes[1] = (0X114 >> 24) & 0xff;
		cusBytes[2] = (0X114 >> 16) & 0xff;
		cusBytes[3] = (0X114 >> 8) & 0xff;
		cusBytes[4] = (0X114 >> 0) & 0xff;
		cusBytes[5] = (regval >> 24) & 0xff;
		cusBytes[6] = (regval >> 16) & 0xff;
		cusBytes[7] = (regval >> 8) & 0xff;
		cusBytes[8] = (regval >> 0) & 0xff;		
		rerr = ParamSet(*prdr, MTR_PARAM_CUSTOM, &cusParam);
		if (rerr != MT_OK_ERR)
		{
			printf("get 0x114 failed\n");
			CloseReader(*prdr);
			return rerr;
		}
		rerr = ParamGet(*prdr, MTR_PARAM_RF_TEMPERATURE, &gLastRfidTemperatrue);
		if (rerr != MT_OK_ERR)
		{
			CloseReader(*prdr);
			return rerr;
		}
		printf("gLastRfidTemperatrue:%d\n", gLastRfidTemperatrue);
	}
	
	return rerr;
}

int gAsync_Inv_Running = 0;

int HandleModErr(int *prdr)
{
	int i;
	printf("enter HandleModErr\n");
	CloseReader(hreader);
	if (echr_istrigger() == 1)
	{
		printf("if (echr_istrigger() == 1)\n");
		gErrSend = gRdrErr;
		return -1;
	}
	
	for (i = 0; i < 3; ++i)
	{
		gRdrErr = OpenReader(prdr);
		if (gRdrErr == MT_OK_ERR)
			break;
	}
	
	gErrSend = gRdrErr;
	gAsync_Inv_Running = 0;
	if (gRdrErr == MT_OK_ERR)
	{
		printf("HandleModErr failed\n");
		return 0;
	}
	else
		return -1;
}

typedef enum 
{
        MidMsgType_None = 0,
        MidMsgType_TagRead = 1,
        MidMsgType_GpiTrigger = 2,
        MidMsgType_TagComing = 3,
        MidMsgType_HeartBeat = 4,
        MidMsgType_RdrError = 5,
        MidMsgType_SyncTimeReq = 6,

        MidMsgType_GetConf = 20,
        MidMsgType_SetConf = 21,
        MidMsgType_GetGPI= 22,
        MidMsgType_SetGPO = 23,
        MidMsgType_Reboot = 24,
	
} MidMsgType;

typedef enum 
{	
	AckClient_NoResp = 0,
	AckClient_Require = 1,	
} AckClient_Mode;

#define TagSendBufLen 384
#define CmdRecvBufLen 512

unsigned char *SockSendBuffer;
unsigned char *SockRecvBuffer;
unsigned char *CmdRespSendBuffer;

#define httpAPIErrCodeBase 100000

int AddMsgHeader2SockBuffer(unsigned char *SBuffer, MidMsgType mtype, int ecode)
{
	int pos = 0;
	int err_ = 0;
	if (ecode != 0)
	{
		if (ecode < httpAPIErrCodeBase)
			err_ = ecode+httpAPIErrCodeBase;
	}
	SBuffer[pos++] = 0xff;
	SBuffer[pos++] = gAppCusParam->name_len;
	pos += 2;
	SBuffer[pos++] = mtype;
	SBuffer[pos++] = 0x00;
	SBuffer[pos++] = (err_ >> 24) & 0xff;
	SBuffer[pos++] = (err_ >> 16) & 0xff;
	SBuffer[pos++] = (err_ >> 8) & 0xff;
	SBuffer[pos++] = (err_ >> 0) & 0xff;
	memcpy(SBuffer+pos, gAppCusParam->name, gAppCusParam->name_len);
	pos += gAppCusParam->name_len;
	return pos;
}
void SetMsgDatalen(unsigned char *SBuffer, int totallen)
{
	SBuffer[2] = ((totallen-10-gAppCusParam->name_len) >> 8) & 0xff;
	SBuffer[3] = ((totallen-10-gAppCusParam->name_len) >> 0) & 0xff;	
}

void AddTagCnt2SockBuffer(unsigned char *SBuffer, int tagcnt, int pos)
{
	SBuffer[pos] = (tagcnt >> 8) & 0xff;
	SBuffer[pos+1] = (tagcnt >> 0) & 0xff;
}

int AddTag2SockBuffer(unsigned char *SBuffer, TAGINFO *tag, int pos)
{
	int nbytes = 0;
	SBuffer[pos+nbytes++] = tag->AntennaID;
	SBuffer[pos+nbytes++] = tag->ReadCnt;
	SBuffer[pos+nbytes++] = (unsigned char)tag->RSSI;
	SBuffer[pos+nbytes++] = 0x05; //protocol
	SBuffer[pos+nbytes++] = tag->Epclen;
	memcpy(SBuffer+pos+nbytes, tag->EpcId, tag->Epclen);
	nbytes += tag->Epclen;
	SBuffer[pos+nbytes++] = 0x00; //bank data len
	
//	memset(SockSendBuffer+pos+nbytes, 0, 8); //firstseen_timestamp
//	nbytes += 8;
//	memset(SockSendBuffer+pos+nbytes, 0, 8); //lastseen_timestamp
//	nbytes += 8;
	return nbytes;
}


int RecvCmd(int rtimeout, int *cmdid, int *datalen)
{
	int commonfd;
	int ret;
	int pos = 0;
	unsigned short rs232rtm = rtimeout / 10;
	
	if (gIsUartSend == 1)
	{
		commonfd = gUart0fd;
		ioctl(gUart0fd, COMMON_INTERFACE_UART_SET_TIMEOUT, &rs232rtm);
	}
	else
	{
		commonfd = fdconnect;
		if (setsockopt(fdconnect, SOL_SOCKET, SO_RCVTIMEO, &rtimeout, sizeof(rtimeout)) != 0)
			return -1;
	}
	
	ret = read(commonfd, SockRecvBuffer+pos, 1);
	if (ret != 1)
		return -1;
	printf("recv first byte\n");
	pos++;
	if (SockRecvBuffer[0] != 0xEE)
	{
		printf("(SockRecvBuffer[0] != 0xEE)\n");
		return -1;
	}
	ret = read_n(commonfd, SockRecvBuffer+pos, 5);
	if (ret != 5)
	{
		printf("recv header failed\n");
		return -1;
	}
	
	pos += 5;
	*datalen = (SockRecvBuffer[2] << 8) | SockRecvBuffer[3];
	*cmdid = SockRecvBuffer[4];
	printf("cmdid:%d, datalen:%d\n", *cmdid, *datalen);
	ret = read_n(commonfd, SockRecvBuffer+pos, *datalen);
	if (ret != *datalen)
	{
		printf("recv body failed\n");
		return -1;
	}
	return 0;
}

void up_send(unsigned char *SBuffer, int dlen);

void ExcRemCmd(int cmdid, int datalen)
{
	int i;
	int pos = 6;
	int resppos = 0;

	switch(cmdid)
	{
		case MidMsgType_GetConf:
		{
			int conflen;
			resppos = AddMsgHeader2SockBuffer(CmdRespSendBuffer, (MidMsgType)cmdid, 0);
			printf("case MidMsgType_GetConf\n");
			if (GetFlashConfig(CmdRespSendBuffer+resppos, &conflen) != 0)
			{
				printf("err MidMsgType_GetConf\n");
				resppos = AddMsgHeader2SockBuffer(CmdRespSendBuffer, (MidMsgType)cmdid, 0x12);
			}
			else
			{
				printf("ok MidMsgType_GetConf\n");
				SetMsgDatalen(CmdRespSendBuffer, conflen+resppos);
				up_send(CmdRespSendBuffer, conflen+resppos);
				return;
			}
			break;
		}
		case MidMsgType_SetConf:
		{
			if (SetFlashConfig(SockRecvBuffer+pos, datalen) < 0)
				resppos = AddMsgHeader2SockBuffer(CmdRespSendBuffer, (MidMsgType)cmdid, 0x07);
			else
				resppos = AddMsgHeader2SockBuffer(CmdRespSendBuffer, (MidMsgType)cmdid, 0);

			break;
		}
		case MidMsgType_GetGPI:
		{
			resppos = AddMsgHeader2SockBuffer(CmdRespSendBuffer, (MidMsgType)cmdid, 0);
			CmdRespSendBuffer[resppos++] = 4;
			CmdRespSendBuffer[resppos++] = 1;
			CmdRespSendBuffer[resppos++] = io_read(GPIO_IN_IN1);
			CmdRespSendBuffer[resppos++] = 2;
			CmdRespSendBuffer[resppos++] = io_read(GPIO_IN_IN2);
			CmdRespSendBuffer[resppos++] = 3;
			CmdRespSendBuffer[resppos++] = io_read(GPIO_IN_IN3);
			CmdRespSendBuffer[resppos++] = 4;
			CmdRespSendBuffer[resppos++] = io_read(GPIO_IN_IN4);
			break;
		}//gpicnt(1 byte), 
		case MidMsgType_SetGPO:
		{
			int gpocnt = SockRecvBuffer[pos++];
			unsigned char gpoid;
			unsigned char gpoids[4];
			int gpoidspos = 0;
			unsigned char gpostates[4];
			int gpostatedurs[4];
			unsigned char gpostate;
			int isSetGPO = 1;
			int j;
			
			if (gpocnt == 0 && gpocnt > 4)
			{
					resppos = AddMsgHeader2SockBuffer(CmdRespSendBuffer, (MidMsgType)cmdid, 0x07);
					isSetGPO = 0;
					gpocnt = 0;
			}
			
			for (i = 0; i < gpocnt; ++i)
			{
				gpoid = SockRecvBuffer[pos++];
				if (gpoid < 1 || gpoid > 4)
				{
					resppos = AddMsgHeader2SockBuffer(CmdRespSendBuffer, (MidMsgType)cmdid, 0x07);
					isSetGPO = 0;
					break;
				}
				else
				{
					gpostate = SockRecvBuffer[pos++];
					if (gpostate > 1)
					{
						resppos = AddMsgHeader2SockBuffer(CmdRespSendBuffer, (MidMsgType)cmdid, 0x07);
						isSetGPO = 0;
						break;			
					}
					else
					{
						gpoids[gpoidspos] = gpoid;
						gpostates[gpoidspos] = gpostate;
						gpostatedurs[gpoidspos] = SockRecvBuffer[pos++];
						gpoidspos++;
					}
				}
			}
			if (isSetGPO == 1)
			{
				int isBreak = 1;
				printf("dump gpo:\n");
				for (j = 0; j < gpoidspos; ++j)
				{
					gpo_set(gpoids[j], gpostates[j]);
					printf("gpoids[%d]:%d\n", j , gpostatedurs[j]);
				}
				printf("\n");
				while(1)
				{
					os_dly_wait(100);
					isBreak = 1;
					for (j = 0; j < gpoidspos; ++j)
					{
						if (gpostatedurs[j] < 0xff)
							gpostatedurs[j]--;

						if (gpostatedurs[j] == 0)
							gpo_set(gpoids[j], 1-gpostates[j]);
					}
					for (j = 0; j < gpoidspos; ++j)
					{
						if (gpostatedurs[j] > 0 && gpostatedurs[j] < 0xff)
							isBreak = 0;
						
						printf("gpoids[%d]:%d\n", j , gpostatedurs[j]);
					}
					printf("\n");
					if (isBreak == 1)
						break;
				}
				resppos = AddMsgHeader2SockBuffer(CmdRespSendBuffer, (MidMsgType)cmdid, 0);
			}				
			break;
		}
		case MidMsgType_Reboot:
		{
			resppos = AddMsgHeader2SockBuffer(CmdRespSendBuffer, (MidMsgType)cmdid, 0);
			SetMsgDatalen(CmdRespSendBuffer, resppos);
			up_send(CmdRespSendBuffer, resppos);
			os_dly_wait(150);
			system_reset();
			return;
		}
		default:
			return;
	}
	
	SetMsgDatalen(CmdRespSendBuffer, resppos);
	up_send(CmdRespSendBuffer, resppos);
}

void up_send(unsigned char *SBuffer, int dlen)
{
	if (SBuffer[4] == MidMsgType_TagRead)
		SBuffer[5] = gAppCusParam->ack_client_mode;
	if (gIsUartSend == 1)
	{
		unsigned short crc ;
		SBuffer[5] |= (1 << 1);
		crc = MsgCRC_16(SBuffer, dlen);
		SBuffer[dlen] = (crc >> 8) & 0xff;
		SBuffer[dlen+1] = (crc >> 0) & 0xff;		
		write(gUart0fd, SBuffer, dlen+2);		
	}
	else
	{
SENDTAGS:		while (1)
		{
			if (Write_N_NoBlk(fdconnect, SBuffer, dlen) == 0)
				break;
		}
		os_dly_wait(5);
	}
	
	if (gAppCusParam->ack_client_mode == AckClient_Require && 
		SBuffer[4] == 0x01)
	{
		int cid;
		int datalen;
		int tmpdur;
		int timeout = 5000;
		unsigned long long startRecvAck = getSysTick();
		
RECVACK:		if (RecvCmd(timeout, &cid, &datalen) == 0)
		{
			if (cid == MidMsgType_TagRead)
			{
				return;
			}
			else
			{
				printf("!!!!!!!!!!!!!!!!!!!!!!  ExcRemCmd\n");
				ExcRemCmd(cid, datalen);
				tmpdur = getSysTick() - startRecvAck;
				timeout -= tmpdur;
				if (timeout < 25) 
					return;
				else
					goto RECVACK;
			}
		}
		goto SENDTAGS;
	}
}

void send_pulse(int justpoweron)
{
	unsigned long long now = getSysTick();
	if (gIsFinInit == 0)
		return;
	
	if ((now - glastpulsetm) > (gAppCusParam->heart_beat_cylce*1000) || 
		justpoweron == 1)
	{
		int pos = AddMsgHeader2SockBuffer(SockSendBuffer, MidMsgType_HeartBeat, gRdrStateFlag);		
		int i;
		
		SockSendBuffer[pos++] = gHbData.main_board;
		SockSendBuffer[pos++] = gHbData.rfid_mod;
		memcpy(SockSendBuffer+pos, gHbData.software_version, 4);
		pos += 4;
		SockSendBuffer[pos++] = gHbData.antcount;
		for (i = 0; i < gHbData.antcount; ++i)
			SockSendBuffer[pos++] = gHbData.connected_antennas[i];
		SockSendBuffer[pos++] = (gHbData.hb_count >> 24) & 0xff;
		SockSendBuffer[pos++] = (gHbData.hb_count >> 16) & 0xff;
		SockSendBuffer[pos++] = (gHbData.hb_count >> 8) & 0xff;
		SockSendBuffer[pos++] = (gHbData.hb_count >> 0) & 0xff;
		
		SetMsgDatalen(SockSendBuffer, pos);

		up_send(SockSendBuffer, pos);
		glastpulsetm = getSysTick();
		gHbData.hb_count++;
	}
}


void send_error()
{
	if (gErrSend != 0)
	{
		int pos = AddMsgHeader2SockBuffer(SockSendBuffer, MidMsgType_RdrError, gErrSend);
	
		gRdrStateFlag = gErrSend;
		gErrSend = 0;
		memset(SockSendBuffer+pos, 0, 24);
		pos += 24;
		SetMsgDatalen(SockSendBuffer, pos);
		up_send(SockSendBuffer, pos);
	}
}

void RemoteCmd(void)
{
	int datalen;
	int cmdid;
	if (RecvCmd(20, &cmdid, &datalen) == 0)
		ExcRemCmd(cmdid, datalen);
}


void send_tags(void)
{
	int i;
	int tagcnt;
	int pos = 0;
	int tagcntpos;
	unsigned long long now;
	if (gIsUartSend == 0)
		CheckServerConnection(&serv_addr);
	send_pulse(1);
	
	while (1)
	{
		RemoteCmd();
		send_pulse(0);
		send_error();
		
		now = getSysTick();
		if ((now - glastsendtagstm) < (gAppCusParam->data_aggr_duration))
			continue;
		glastsendtagstm = now;
			
		tagcnt = tagGetCnt();
		printf("before GetTagCountInBuffer tagcnt:%d\n", tagcnt);
		if (tagcnt > 0)
		{
			int tagbatchcnt = 0;
			TAGINFO tmpTag;
			pos = AddMsgHeader2SockBuffer(SockSendBuffer, MidMsgType_TagRead, 0);
			tagcntpos = pos;
			pos += 2;
			for (i = 0; i < tagcnt; ++i)
			{
				tagGetNext(&tmpTag);
				pos += AddTag2SockBuffer(SockSendBuffer, &tmpTag, pos);
				tagbatchcnt++;
				if (pos + gAppCusParam->max_rec_databytes_length + 30 >= TagSendBufLen)
				{
					AddTagCnt2SockBuffer(SockSendBuffer, tagbatchcnt, tagcntpos);
					SetMsgDatalen(SockSendBuffer, pos);
//					printf("up_send tagbatchcnt:%d, size:%d\n", tagbatchcnt, pos);
					up_send(SockSendBuffer, pos);
					pos = AddMsgHeader2SockBuffer(SockSendBuffer, MidMsgType_TagRead, 0);
					tagbatchcnt = 0;
					pos = tagcntpos + 2;
				}
			}
			if (tagbatchcnt != 0)
			{
				AddTagCnt2SockBuffer(SockSendBuffer, tagbatchcnt, tagcntpos);
				SetMsgDatalen(SockSendBuffer, pos);
//				printf("last up_send tagbatchcnt:%d, size:%d\n", tagbatchcnt, pos);
				up_send(SockSendBuffer, pos);
			}
		}
	}
}

int CaliR2000Register(void)
{
	if (gIsCaliR2000Reg == 1)
	{
		int getTemperDur;
		unsigned long long nowtm = getSysTick();
		
		if (nowtm - gInitGetTemperTime < 300000)
//			getTemperDur = 15000;
			getTemperDur = 2000;
		else
//			getTemperDur = 120000;
			getTemperDur = 4000;
		if (nowtm - gLastGetTemperTime > getTemperDur)
		{
			int nowRfidTemperature;
			int diffTemper;		
			
//			printf("CaliR2000Register start:%d\n", getSysTickU32());
			gLastGetTemperTime = nowtm;
			if (gAppCusParam->inv_cycle == 0)
			{
				gRdrErr = SyncStopFastReading(hreader);
				if (gRdrErr != MT_OK_ERR)
					return -1;
			}
			gRdrErr = ParamGet(hreader, MTR_PARAM_RF_TEMPERATURE, &nowRfidTemperature);
			if (gRdrErr != MT_OK_ERR)
				return -1;
//			printf("start CaliR2000Register now temp:%d\n", nowRfidTemperature);
			diffTemper = nowRfidTemperature - gLastRfidTemperatrue;
			if (diffTemper >= 5 || diffTemper <= -5)
			{
				CustomParam_ST cusParam;
				unsigned char cusBytes[10];
				unsigned int regval;
				
//				printf("set register diffTemper:%d\n", diffTemper);
				gLastRfidTemperatrue = nowRfidTemperature;
				cusParam.CParamlen = 10;
				cusParam.pCusParam = cusBytes;
				cusBytes[0] = 0x03;
				cusBytes[1] = (0X114 >> 24) & 0xff;
				cusBytes[2] = (0X114 >> 16) & 0xff;
				cusBytes[3] = (0X114 >> 8) & 0xff;
				cusBytes[4] = (0X114 >> 0) & 0xff;
				
				gRdrErr = ParamGet(hreader, MTR_PARAM_CUSTOM, &cusParam);
				if (gRdrErr != MT_OK_ERR)
					return -1;
//				printf("0X114 value:%d\n", cusBytes[3]);
				cusBytes[8] = cusBytes[3];
				cusBytes[0] = 0x03;
				cusBytes[1] = (0X114 >> 24) & 0xff;
				cusBytes[2] = (0X114 >> 16) & 0xff;
				cusBytes[3] = (0X114 >> 8) & 0xff;
				cusBytes[4] = (0X114 >> 0) & 0xff;				
				memset(cusBytes+5, 0, 3);
				gRdrErr = ParamSet(hreader, MTR_PARAM_CUSTOM, &cusParam);
				if (gRdrErr != MT_OK_ERR)
					return -1;
				os_dly_wait(1);
				cusBytes[0] = 0x03;
				cusBytes[1] = (0X115 >> 24) & 0xff;
				cusBytes[2] = (0X115 >> 16) & 0xff;
				cusBytes[3] = (0X115 >> 8) & 0xff;
				cusBytes[4] = (0X115 >> 0) & 0xff;	
				gRdrErr = ParamGet(hreader, MTR_PARAM_CUSTOM, &cusParam);
				if (gRdrErr != MT_OK_ERR)
					return -1;
//				printf("0X115 value:%d\n", cusBytes[3]);
				regval = cusBytes[3] | 0x100;
				cusBytes[0] = 0x03;
				cusBytes[1] = (0X114 >> 24) & 0xff;
				cusBytes[2] = (0X114 >> 16) & 0xff;
				cusBytes[3] = (0X114 >> 8) & 0xff;
				cusBytes[4] = (0X114 >> 0) & 0xff;
				cusBytes[5] = (regval >> 24) & 0xff;
				cusBytes[6] = (regval >> 16) & 0xff;
				cusBytes[7] = (regval >> 8) & 0xff;
				cusBytes[8] = (regval >> 0) & 0xff;		
				gRdrErr = ParamSet(hreader, MTR_PARAM_CUSTOM, &cusParam);
				if (gRdrErr != MT_OK_ERR)
					return -1;
			}
//			printf("CaliR2000Register end:%d\n", getSysTickU32());
			return 1;
		}
	}
	return 0;
}

extern int IsCloseSockReset;

typedef enum
{
	TriStCode_WaitStart = 0,
	TriStCode_WaitStop = 1,
	TriStCode_WaitTimeout = 2,
} TriggerStateCode;

int ContainCond(int type)
{
	int gpistates[4];
	int i;
	int met = 1;
	if (type == 1)
	{
		gpistates[0] = io_read(GPIO_IN_IN1);
		gpistates[1] = io_read(GPIO_IN_IN2);
		gpistates[2] = io_read(GPIO_IN_IN3);
		gpistates[3] = io_read(GPIO_IN_IN4);
		
		for (i = 0; i < gAppCusParam->gpi_trigger1.gpi_count; ++i)
		{
			if (gpistates[gAppCusParam->gpi_trigger1.gpi_ids[i]] != gAppCusParam->gpi_trigger1.gpi_states[i])
			{
				met = 0;
				break;
			}
		}
		
		if (met == 0 && gAppCusParam->gpi_trigger_mode == 3)
		{
			
		}
		
	}
	else
	{
	}
	return met;
}


#define GPI_CHECK_CYCLE 20

void StartAsyncInventory()
{
	if (gAsync_Inv_Running == 0)
	{
		gRdrErr = SyncStartFastReading(hreader, gConnants.connectedants, gConnants.antcnt, 
			(((0x1 << 1) | (0x1 << 2) | (0x1 << 3)) << 8) | 0x80 | (0x01 << 24));
		if (gRdrErr == MT_OK_ERR)
			gAsync_Inv_Running = 1;
	}
}

void user_main_active(void)
{
	commonUartPara uart0Para;
//	baseParaConfig config;
	int tagcnt;
	int i;
	TAGINFO tag;
	int realstksize;
	U64 * pstk;
	TriggerStateCode triStrate = TriStCode_WaitStart;
	int ret = 0;
	unsigned long long lastChkGpiTime;
	gLastGetTemperTime= getSysTick();
	gInitGetTemperTime= gLastGetTemperTime;
	
	io_write(GPIO_OUT_O1, 1);
	io_write(GPIO_OUT_O2, 1);
	io_write(GPIO_OUT_O3, 1);
	io_write(GPIO_OUT_O4, 1);
	
	led_1on();
	IsCloseSockReset = 0;
	
	uart0Para.isBlock	= O_BLOCK;
	uart0Para.isPrintf	= 1;
	uart0Para.baudrate	= 115200;
	uart0Para.mode		= AT91C_US_USMODE_NORMAL;
	uart0Para.databits	= AT91C_US_CHRL_8_BITS;
	uart0Para.stopbits	= AT91C_US_NBSTOP_1_BIT;
	uart0Para.timeout	= 2;
	gUart0fd = open(COMMON_INTERFACE_UART0,&uart0Para);
	
	SockSendBuffer = malloc(TagSendBufLen);
	SockRecvBuffer = malloc(CmdRecvBufLen);
	CmdRespSendBuffer = malloc(CmdRecvBufLen);
	
	gAppCusParam = malloc(sizeof(AppCustomParams));
	
	ret = ReadAppCustomParams(gAppCusParam);
//	SetDefCustomParams(gAppCusParam);
	
	gHbData.main_board = 1;
	firmware_version(gHbData.software_version);	
	gHbData.hb_count = 0;
	
	if (ret != 0)
	{
		reason1 = 0;
		goto FIN;
	}
	else 
	{	
		gAppCusParam->cusparam[gAppCusParam->cusparam_len] = 0;
		printf("gAppCusParam->cusparam:%s\n", gAppCusParam->cusparam);
		if (strstr((char*)gAppCusParam->cusparam, "CaliReg") != NULL)
			gIsCaliR2000Reg = 1;
		
		if (gAppCusParam->upload_ip[0] == 0)
		{
			gIsUartSend = 1;
		}
		DumpAppCustomParams(gAppCusParam);
	}

//	gAppCusParam.max_rec_databytes_length = 16;
	initTbBuffer(gAppCusParam->max_rec_databytes_length);
	echr_init(90);
	
	
	if (gAppCusParam->upload_ip[0] == 0)
	{
//		close(gUart0fd);
		uart0Para.isBlock	= O_BLOCK;
		uart0Para.isPrintf	= 0;
		uart0Para.baudrate	= (gAppCusParam->upload_ip[1] << 16) | 
			(gAppCusParam->upload_ip[2] << 8) | gAppCusParam->upload_ip[3];
		uart0Para.mode		= AT91C_US_USMODE_NORMAL;
		uart0Para.databits	= AT91C_US_CHRL_8_BITS;
		uart0Para.stopbits	= AT91C_US_NBSTOP_1_BIT;
		uart0Para.timeout	= 4;
		gUart0fd = open(COMMON_INTERFACE_UART0,&uart0Para);	
	}
	else
	{
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family=AF_INET;
		serv_addr.sin_port=htons(gAppCusParam->upload_port);
		serv_addr.sin_addr.s_addr = (gAppCusParam->upload_ip[3] << 24) | 
			(gAppCusParam->upload_ip[2] << 16) | (gAppCusParam->upload_ip[1] << 8) | 
			gAppCusParam->upload_ip[0];
	}
	
	printf("enter  main 10.0.0.15.011\n");

	glastpulsetm = getSysTick();
	glastsendtagstm = glastpulsetm;
	
	////////
	pstk = align8byte(malloc(1024*2), 1024*2, &realstksize);
	os_tsk_create_user(send_tags, 10, pstk, realstksize);
	/////////

	for (i = 0; i < 3; ++i)
	{
//		printf("reconf:%d\n", reconf);
		gRdrErr = OpenReader(&hreader);
		printf("------ gRdrErr:%d\n", gRdrErr);
		if (gRdrErr == MT_OK_ERR)
			break;
	}
	
	if (gRdrErr != MT_OK_ERR)
	{
		reason1 = 1;
		gErrSend = (int)gRdrErr;
		gIsFinInit = 1;
		goto FIN;
	}
	gIsFinInit = 1;
	
	led_allon();
		
	{
WAIT_TRI:
		if (gAppCusParam->is_gpi_trigger == 1)
		{
			while (1)
			{
				if (ContainCond(1) == 1)
				{
					lastChkGpiTime = getSysTick();
					break;
				}
				os_dly_wait(2);
			}
		}
		
DO_ASYNC_INV:
		if (gAppCusParam->inv_cycle == 0)
			StartAsyncInventory();
		else
		{
			CaliR2000Register();
			gRdrErr = TagInventory_Raw(hreader, gConnants.connectedants, gConnants.antcnt, 
				gAppCusParam->inv_cycle*gConnants.antcnt, &tagcnt);			
		}
		
		if (gRdrErr == MT_OK_ERR || gRdrErr == MT_CMD_NO_TAG_ERR)
		{
			if (gAppCusParam->inv_cycle == 0)
			{
				gRdrErr = SyncGetNextTag(hreader, &tag);
				if (gRdrErr == MT_OK_ERR || gRdrErr == MT_CMD_NO_TAG_ERR)
				{
						if (tag.Epclen > 0 && tag.Epclen >= gAppCusParam->max_rec_databytes_length)
							tagInsert(&tag);
				}
			}
			else
			{
				for (i = 0; i < tagcnt; ++i)
				{
					gRdrErr = GetNextTag(hreader, &tag);
					if (gRdrErr == MT_OK_ERR || gRdrErr == MT_CMD_NO_TAG_ERR)
					{
						if (tag.Epclen > 0 && tag.Epclen >= gAppCusParam->max_rec_databytes_length)
							tagInsert(&tag);
					}
					else
						break;
				}
			}
			if (!(gRdrErr == MT_OK_ERR || gRdrErr == MT_CMD_NO_TAG_ERR))
			{
				if (HandleModErr(&hreader) != 0)
				{
					reason1 = 3;
					goto FIN;
				}
			}
		}
		else
		{
			if (HandleModErr(&hreader) != 0)
			{
				reason1 = 3;
				goto FIN;
			}	
		}
		
		if (gAppCusParam->is_gpi_trigger == 1)
		{
			if (getSysTick() - lastChkGpiTime >= GPI_CHECK_CYCLE)
			{
				lastChkGpiTime = getSysTick();
				if (ContainCond(0) == 1)
				{
					if (gAppCusParam->inv_cycle == 0)
					{
						gRdrErr = SyncStopFastReading(hreader);
						if (gRdrErr != MT_OK_ERR)
						{
							if (HandleModErr(&hreader) != 0)
							{
								reason1 = 3;
								goto FIN;
							}
						}
						else
							gAsync_Inv_Running = 0;
					}
					goto WAIT_TRI;
				}
			}
		}
		
		if (gAppCusParam->inv_cycle != 0)
			os_dly_wait(gAppCusParam->interval_cycle / 10);
		
		goto DO_ASYNC_INV;
	}

FIN:	
	led_1on();
	while(1)
	{
		os_dly_wait(100);
		/*
		if (reason1 == 0)
			printf("no configuration\n");
		else if (reason1 == 1)
			printf("user_main Init Reader err:%d\n", gRdrErr);
		else if (reason1 == 2)
			printf("user_main TagInventory_Raw err:%d\n", gRdrErr);
		else if (reason1 == 3)
			printf("user_main GetNextTag err:%d\n", gRdrErr);
		else if (reason1 == 4)
			printf("user_main create socket error\n");
		else if (reason1 == 5)
			printf("user_main connect  error\n");
		else
			printf("user_main Unknown reason\n");*/
	}
	
//	os_dly_wait(200);
	
//	system_reset();	
	/*
	while(1)
	{
		os_dly_wait(100);
	}
	*/
}

void user_main_passive(void);
extern volatile int uart0fd;
void user_main(void)
{
	/*
	commonUartPara uart0Para;
	uart0Para.isBlock	= O_BLOCK;
	uart0Para.isPrintf	= 1;
	uart0Para.baudrate	= 115200;
	uart0Para.mode		= AT91C_US_USMODE_NORMAL;
	uart0Para.databits	= AT91C_US_CHRL_8_BITS;
	uart0Para.stopbits	= AT91C_US_NBSTOP_1_BIT;
	uart0Para.timeout	= 0xffff;
	uart0fd = open(COMMON_INTERFACE_UART0,&uart0Para);
	*/
	
	if (TestFwType() == 0)
	{
		printf("run user_main_passive");
		user_main_passive();
	}
	else
	{
		printf("run user_main_active");
		user_main_active();
	}
//	user_main_active();
}
