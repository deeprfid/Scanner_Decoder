#include <stdio.h>
#include <string.h>
#include "reader_msg.h"
#include "mp_pool.h"
#include "app_conf.h"
#include "http_callback.h"
#include "mqtt_interface.h"
#include "event_mq.h"
#include "ipc.h"
int gSocketConnected = 0;
uint8 gSerIp[4];
uint16 gSerPort;

#if Custom_By_CDZNWL
void SetMsgEnd(unsigned char *SBuffer)
{
	char *sbuff = (char *)SBuffer;
	sbuff[strlen(sbuff)-1] = '#';
}
#endif

int EEcmd_reply(int fd, uint8 *SBuffer, int dlen)
{
	write_n(fd, SBuffer, dlen);
	return 0;
}

int write_n(int fd, void *buf, int len)
{
	int pos = 0;
	int nleft = len;
	int nret = 0;
	unsigned char *pbuf = buf;
	int errcnt = 0;
	
	while (nleft > 0)
	{
		nret = write(fd, pbuf+pos, nleft);
		if (nret <= 0)
		{			
			if (nret == 0)
			{
				sleep_ms(100);
				errcnt++;
				if (errcnt == 5)
					return -1;
				else
					continue;
			}
			else
				return -1;
		}
		else
		{
			pos += nret;
			nleft -= nret;
		}
	}
	return len;
}

void CheckServerConnection(void)
{
	uint8_t iomode = SOCK_IO_NONBLOCK;
	TRACE("enter CheckServerConnection\n");

	while (1)
	{
		if (socket(COMMON_INTERFACE_SOCKET0, Sn_MR_TCP, 0, SF_TCP_NODELAY) < 0)
		{
			TRACE("connect server reset by create socket\n");
			system_reset();
		}
		TRACE("before connect server\n");
		if (connect(COMMON_INTERFACE_SOCKET0, gSerIp, gSerPort) != SOCK_OK)
		{
			TRACE("after connect failed\n");
			close(COMMON_INTERFACE_SOCKET0);
			sleep_ms(2000);
			continue;
		}
		else
		{
			TRACE("connect server successfully\n");			
			if (ctlsocket(COMMON_INTERFACE_SOCKET0, CS_SET_IOMODE, &iomode) != SOCK_OK)
			{
				TRACE("ctlsocket error\n");
				system_reset();
			}
			gSocketConnected = 1;			
			break;
		}
	}
}
/*
int Write_N_NoBlk(int fd, unsigned char *buf, int len)
{
	int pos = 0;
	int nleft = len;
	int nret = 0;
	unsigned char *pbuf = buf;
	int errcnt = 0;
	int ret = 0;
	
//	printf("enter Write_N_NoBlk\n");	
	while (nleft > 0)
	{
		nret = write(fd, pbuf+pos, nleft);
		if (nret <= 0)
		{
			printf("write pos:%d, nret:%d\n", pos, nret);	
			if (nret == 0)
			{
				sleep_ms(100);
				errcnt++;
				if (errcnt == 5)
				{
					ret = 1;
					goto WRET;				
				}
				else
					continue;
			}
			else
			{
				ret = 1;
				goto WRET;	
			}
		}
		else
		{
			pos += nret;
			nleft -= nret;
		}
	}
	
WRET:
	if (ret != 0)
	{
		printf("before close\n");
		disconnect(fd);
		close(fd);
		sleep_ms(2000);
		printf("after close\n");
		CheckServerConnection();		
	}
	return ret;
}
*/
extern uint8 gLastHttpFailed;
uint16 gContiSockFaidedCnt = 0;
void sock_uart_upload(unsigned char *SBuffer, int dlen)
{
	int sfd;
	int slen = dlen;
	int rtimeout = gRtSetting->upload.recv_timeout*1000;
	int pos = 0;
	int datalen;
	int iscrc;
	
	unsigned short crc ;
#if Custom_By_CDZNWL || Custom_By_GZTD || Custom_By_HZWXZN
#else
	SBuffer[5] = 0;
	if (SBuffer[4] == MidMsgType_TagRead)
		SBuffer[5] |= gRtSetting->upload.client_ack;
	
	if (gRtSetting->upload.crc_enable == 1 || 
		gRtSetting->upload.hw_inf == Upload_Inf_Uart_1 || 
		gRtSetting->upload.hw_inf == Upload_Inf_Uart_2)
	{
		SBuffer[5] |= (1 << 1);
		crc = crc_Msg(SBuffer, dlen);
		SBuffer[dlen] = (crc >> 8) & 0xff;
		SBuffer[dlen+1] = (crc >> 0) & 0xff;
		slen += 2;
	}
#endif
    
	if (gRtSetting->upload.hw_inf == Upload_Inf_Uart_1)
		sfd = COMMON_INTERFACE_UART2;
	else if (gRtSetting->upload.hw_inf == Upload_Inf_Uart_2)
		sfd = COMMON_INTERFACE_UART3;
	else if (gRtSetting->upload.hw_inf == Upload_Inf_4G || 
		gRtSetting->upload.hw_inf == Upload_Inf_Wifi)
		sfd = COMMON_INTERFACE_UART1;
	else if (gRtSetting->upload.hw_inf == Upload_Inf_Ethernet)
		sfd = COMMON_INTERFACE_SOCKET0;
	
Reconn:
	if (gContiSockFaidedCnt >= MaxUpsendFailedCntBefReset)
		reset_uart1_ex_dev(&gContiSockFaidedCnt);
		
	if (gRtSetting->upload.hw_inf == Upload_Inf_Ethernet 
		&& gSocketConnected == 0)
	{
		disconnect(sfd);
		close(sfd);
		CheckServerConnection();
	}
		
	if (write_n(sfd, SBuffer, slen) != slen)
	{
		TRACE("if (write_n(sfd, SBuffer, slen) != slen)\n");
		gSocketConnected = 0;		
		goto Reconn;
	}
	
	if (gRtSetting->upload.client_ack == 1 && 
		SBuffer[4] == MidMsgType_TagRead)
	{
RecvAck:
		pos = 0;
		ioctl(sfd, COMMON_INTERFACE_SET_TIMEOUT, &rtimeout);

		if (read(sfd, SockRecvBuffer+pos, 1) != 1)
		{
			TRACE("if (read(sfd, SockRecvBuffer+pos, 1) != 1)\n");
			gLastHttpFailed = 1;
			gContiSockFaidedCnt++;
			goto Reconn;
		}
		
		pos++;
		if (SockRecvBuffer[0] != 0xEE)
		{
			TRACE("(SockRecvBuffer[0] != 0xEE), %02X\n", SockRecvBuffer[0]);
			goto RecvAck;
		}
		
		if (read_n(sfd, SockRecvBuffer+pos, 5) != 5)
		{
			TRACE("if (read_n(sfd, SockRecvBuffer+pos, 5) != 5)\n");
			gLastHttpFailed = 1;
			gContiSockFaidedCnt++;
			goto Reconn;
		}		
		pos += 5;
		
		if (SockRecvBuffer[4] != MidMsgType_TagRead)
		{
			custom_ee_commond(sfd, SockRecvBuffer, 3, NULL, EEcmd_reply, crc_Msg, gRtSetting);
			TRACE("msgtype:%d,  1!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", SockRecvBuffer[4]);
			goto RecvAck;
		}
		
		datalen = (SockRecvBuffer[2] << 8) | SockRecvBuffer[3];
		iscrc = (SockRecvBuffer[5] >> 1) & 0x01;
		if (iscrc == 1)
			datalen += 2;
		
		if (read_n(sfd, SockRecvBuffer+pos, datalen) != datalen)
		{
			TRACE("if (read_n(sfd, SockRecvBuffer+pos, datalen) != datalen)\n");
			gLastHttpFailed = 1;
			gContiSockFaidedCnt++;
			goto Reconn;
		}
		
		if (gLastHttpFailed == 1 && gRtSetting->upload.hw_inf != Upload_Inf_Ethernet)
		{
			sleep_ms(gRtSetting->upload.clr_r_buf_time*1000);
			ioctl(sfd, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
		}
		gLastHttpFailed = 0;
		gContiSockFaidedCnt = 0;
//		TRACE("recv ack ok ------------------------------\n");
	}
}

void up_send(unsigned char *SBuffer, int dlen)
{
	if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Http)
	{
			http_upload(SBuffer, dlen);
	}	
	else if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Mqtt)
		mqtt_task(SBuffer, dlen);
	else
		sock_uart_upload(SBuffer, dlen);
}
#if Custom_By_Caipan
char gMsg_IPStr[20];
char gMsg_MACStr[20];
#endif
//int gHttpSendToken = 0;
int AddMsgHeader2SockBuffer_j(char *Jbuf, MidMsgType mtype)
{
	char evtstr[30];

#if Custom_By_Caipan
	switch (mtype)
	{
		case MidMsgType_TagRead:
			strcpy(evtstr, "20001");
			break;
		case MidMsgType_HeartBeat:
			strcpy(evtstr, "20003");
			break;
		case MidMsgType_SyncTimeReq:
			strcpy(evtstr, "20002");
			break;
		default:
			return -1;
	}
	
	sprintf(Jbuf, "{\"cmd\":\"%s\",\"data\":{\"ip\":\"%s\",\"mac\":\"%s\",\"deviceNo\":\"%s\"",
		evtstr, gMsg_IPStr, gMsg_MACStr, gRtSetting->glob_params.name);
	
	if (mtype == MidMsgType_TagRead)
		strcat(Jbuf, ",\"epcs\":[");
#elif Custom_By_ZHXX_ZSYH
	strcpy(Jbuf, "[");
#else
	switch (mtype)
	{
		case MidMsgType_TagRead:
			strcpy(evtstr, "tag_read");
			break;
		case MidMsgType_HeartBeat:
			strcpy(evtstr, "heart_beat");
			break;
		case MidMsgType_RdrError:
			strcpy(evtstr, "reader_exception");
			break;
		case MidMsgType_GpiTrigger:
			strcpy(evtstr, "gpi_changed");
			break;
		case MidMsgType_TagComing:
			strcpy(evtstr, "tag_coming");
			break;
		case MidMsgType_SyncTimeReq:
			strcpy(evtstr, "sync_time_req");
			break;
		default:
			return -1;
	}
	
	sprintf(Jbuf, "{\"reader_name\":\"%s\",\"event_type\":\"%s\",\"event_data\":",
		gRtSetting->glob_params.name, evtstr);
//	sprintf(Jbuf, "{\"reader_name\":\"%s\",\"event_type\":\"%s\",\"token\":%d,\"event_data\":",
//		gRtSetting->glob_params.name, evtstr, gHttpSendToken);
	if (mtype == MidMsgType_TagRead || mtype == MidMsgType_TagComing)
		strcat(Jbuf, "[");
#endif
	return 0;
}

int AddMsgHeader2SockBuffer(unsigned char *SBuffer, MidMsgType mtype, int ecode)
{
	int pos = 0;
	int err_ = 0;
	int namelen = strlen(gRtSetting->glob_params.name);
	
	if (ecode != 0)
	{
		if (ecode < httpAPIErrCodeBase)
			err_ = ecode+httpAPIErrCodeBase;
	}
#if Custom_By_CDZNWL
	sprintf((char *)SBuffer, "%s|%02X|00|%08X|", gRtSetting->glob_params.name, 
		mtype, err_);
	return strlen((char *)SBuffer);
#elif Custom_By_GZTD || Custom_By_HZWXZN
	SBuffer[0] = 0;
	return 0;
#else
	SBuffer[pos++] = 0xff;
	SBuffer[pos++] = namelen;
	pos += 2;
	SBuffer[pos++] = mtype;
	SBuffer[pos++] = 0x00;
	SBuffer[pos++] = (err_ >> 24) & 0xff;
	SBuffer[pos++] = (err_ >> 16) & 0xff;
	SBuffer[pos++] = (err_ >> 8) & 0xff;
	SBuffer[pos++] = (err_ >> 0) & 0xff;
	memcpy(SBuffer+pos, gRtSetting->glob_params.name, namelen);
	pos += namelen;
	return pos;
#endif
}

void SetMsgDatalen(unsigned char *SBuffer, int totallen)
{
#if Custom_By_GZTD || Custom_By_HZWXZN
	char *str = (char *)SBuffer;
#if Custom_By_GZTD
	str[strlen(str)-1] = 0;
#endif
#else
	int namelen = strlen(gRtSetting->glob_params.name);
	SBuffer[2] = ((totallen-10-namelen) >> 8) & 0xff;
	SBuffer[3] = ((totallen-10-namelen) >> 0) & 0xff;
#endif
}

void AddTagCnt2SockBuffer(unsigned char *SBuffer, int tagcnt, int pos)
{
#if Custom_By_GZTD || Custom_By_HZWXZN
#else
	SBuffer[pos] = (tagcnt >> 8) & 0xff;
	SBuffer[pos+1] = (tagcnt >> 0) & 0xff;
#endif
}

volatile uint32 gUtcSecBase = 0;
volatile uint32 gSysSecBase = 0;

uint32 get_utc_secs(uint32 syssecs)
{
	return syssecs - gSysSecBase + gUtcSecBase;
}

void seconds_to_date(time_t secs, char *date)
{
	struct tm *pt;
	
	pt = localtime(&secs);
	pt->tm_year += 1900;
	pt->tm_mon  += 1;
	
	sprintf(date, "%d-%02d-%02d %02d:%02d:%02d", pt->tm_year, 
		pt->tm_mon, pt->tm_mday, pt->tm_hour, pt->tm_min, 
		pt->tm_sec);
}

void reset_uart1_ex_dev(uint16 *failcnt)
{
	if (gRtSetting->upload.hw_inf == Upload_Inf_4G || 
		gRtSetting->upload.hw_inf == Upload_Inf_Wifi)
	{
		ex_power_off();
		sleep_ms(1000);
		ex_power_on();
		sleep_ms(15000);
		*failcnt = 0;
		ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
//		TRACE("!!!!!!!!!!!!!!!!!!!  reset_uart1_ex_dev");
	}
}

int date_to_seconds(char *date, uint32 *secs)
{
	char *tmp;
	struct tm tst;
	time_t timestamp;
	
	tst.tm_year = atoi_Arm(date)-1900;
	tmp = strstr(date, "-");
	if (tmp == NULL)
		return -1;
	
	tst.tm_mon = atoi_Arm(tmp+1) -1;
	tmp = strstr(tmp+1, "-");
	if (tmp == NULL)
		return -1;
	
	tst.tm_mday = atoi_Arm(tmp+1);
	tmp = strstr(tmp+1, " ");
	if (tmp == NULL)
		return -1;
	
	while(*tmp == ' ')
	{
		tmp++;
	}
	
	tst.tm_hour = atoi_Arm(tmp);
	tmp = strstr(tmp+1, ":");
	if (tmp == NULL)
		return -1;
	
	tst.tm_min = atoi_Arm(tmp+1);
	tmp = strstr(tmp+1, ":");
	if (tmp == NULL)
		return -1;
	
	tst.tm_sec = atoi_Arm(tmp+1);
	tst.tm_isdst = -1;
	
	timestamp = mktime(&tst);
	*secs = timestamp;

	return 0;
}

int AddTag2SockBuffer_j(char *Jbuf, TAGINFO *tag)
{
	char tmpbuf[128];
	char *jpos = Jbuf + strlen(Jbuf);
#if Custom_By_Caipan
	strcat(jpos, "{\"epc\":\"");
	Hex2Str(tag->EpcId, tag->Epclen, tmpbuf);			
	strcat(jpos, tmpbuf);
	seconds_to_date(get_utc_secs(tag->TimeStamp), tmpbuf);
	if (tag->protocol == 5)
		tag->protocol = (SL_TagProtocol)0;
	sprintf(jpos+strlen(jpos), "\",\"rssi\":%d,\"alarmTime\":\"%s\",\"readCount\":%d,\"direction\":%d},", 
		(signed char)((uint8)(tag->RSSI)), tmpbuf, tag->ReadCnt, tag->protocol);

#elif Custom_By_ZHXX_ZSYH
	Hex2Str(tag->EpcId, tag->Epclen, tmpbuf);
	sprintf(jpos, "\"%s\",", tmpbuf);
#else	
	uint64 ftm_64;
	uint64 ltm_64;
	
	if ((gRtSetting->tag_json_format & 0x8000) == 0x8000)
	{
		strcat(jpos, "{\"epc\":\"");
		Hex2Str(tag->EpcId, tag->Epclen, tmpbuf);			
		strcat(jpos, tmpbuf);
		strcat(jpos, "\",\"bank_data\":\"");
		Hex2Str(tag->EmbededData, tag->EmbededDatalen, tmpbuf);
		strcat(jpos, tmpbuf);
		strcat(jpos, "\",\"antenna\":");
		sprintf(tmpbuf, "%d", tag->AntennaID);
		strcat(jpos, tmpbuf);
		strcat(jpos, ",\"read_count\":");
		sprintf(tmpbuf, "%d", tag->ReadCnt);
		strcat(jpos, tmpbuf);
		strcat(jpos, ",\"protocol\":");
		sprintf(tmpbuf, "%d", tag->protocol);
		strcat(jpos, tmpbuf);
		strcat(jpos, ",\"rssi\":");
		sprintf(tmpbuf, "%d", (signed char)((uint8)(tag->RSSI)));
		strcat(jpos, tmpbuf);
		ftm_64 = (uint64)get_utc_secs(tag->TimeStamp)*(uint64)1000;
		ltm_64 = (uint64)get_utc_secs(tag->Phase)*(uint64)1000;
		sprintf(jpos+strlen(jpos), ",\"firstseen_timestamp\":%lld,\"lastseen_timestamp\":%lld},",
			ftm_64, ltm_64);
	}
	else
	{
		Hex2Str(tag->EpcId, tag->Epclen, tmpbuf);
		sprintf(jpos, "{\"ep\":\"%s\"", tmpbuf);
		
		if ((gRtSetting->tag_json_format & 0x01) == 0x01)
		{
			Hex2Str(tag->EmbededData, tag->EmbededDatalen, tmpbuf);
			sprintf(jpos+strlen(jpos), ",\"bd\":\"%s\"", tmpbuf);
		}
		
		if ((gRtSetting->tag_json_format & 0x02) == 0x02)
			sprintf(jpos+strlen(jpos), ",\"at\":%d", tag->AntennaID);		

		if ((gRtSetting->tag_json_format & 0x04) == 0x04)
			sprintf(jpos+strlen(jpos), ",\"rc\":%d", tag->ReadCnt);	

		if ((gRtSetting->tag_json_format & 0x08) == 0x08)
			sprintf(jpos+strlen(jpos), ",\"fq\":%d", tag->Frequency);	
			
		if ((gRtSetting->tag_json_format & 0x10) == 0x10)
			sprintf(jpos+strlen(jpos), ",\"pt\":%d", tag->protocol);	
		
		if ((gRtSetting->tag_json_format & 0x20) == 0x20)
			sprintf(jpos+strlen(jpos), ",\"ri\":%d", (signed char)((uint8)(tag->RSSI)));	

		if ((gRtSetting->tag_json_format & 0x40) == 0x40)
			sprintf(jpos+strlen(jpos), ",\"rv\":%u", 
				(uint32)((GetNumU16(tag->Res) << 16) | GetNumU16(tag->CRC)));	

		if ((gRtSetting->tag_json_format & 0x80) == 0x80)
		{
			ftm_64 = (uint64)get_utc_secs(tag->TimeStamp)*(uint64)1000;
			sprintf(jpos+strlen(jpos), ",\"ft\":%lld", ftm_64);	
		}
		
		if ((gRtSetting->tag_json_format & 0x100) == 0x100)
		{
			ltm_64 = (uint64)get_utc_secs(tag->Phase)*(uint64)1000;
			sprintf(jpos+strlen(jpos), ",\"lt\":%lld", ltm_64);
		}
		strcat(jpos, "},");
	}
#endif
	return 0;
}
	
int AddTag2SockBuffer(unsigned char *SBuffer, TAGINFO *tag, int pos)
{
#if Custom_By_CDZNWL
	char *sbuff = (char *)SBuffer;
	Hex2Str(tag->EpcId, tag->Epclen, sbuff+strlen(sbuff));
	strcat(sbuff, ",");
	return strlen(sbuff);
#elif Custom_By_GZTD || Custom_By_HZWXZN
	char tmpbuf[128];
	char *jpos = (char *)SBuffer;
	
	Hex2Str(tag->EpcId, tag->Epclen, tmpbuf);			
	sprintf(jpos+strlen(jpos), "%s,", tmpbuf);
	return strlen(jpos);	
#else
	int nbytes = 0;
	SBuffer[pos+nbytes++] = tag->AntennaID;
	SBuffer[pos+nbytes++] = tag->ReadCnt;
	SBuffer[pos+nbytes++] = (unsigned char)tag->RSSI;
	SBuffer[pos+nbytes++] = tag->protocol;
	SBuffer[pos+nbytes++] = tag->Epclen;
	memcpy(SBuffer+pos+nbytes, tag->EpcId, tag->Epclen);
	nbytes += tag->Epclen;
	
	SBuffer[pos+nbytes++] = tag->EmbededDatalen; //bank data len
	memcpy(SBuffer+pos+nbytes, tag->EmbededData, tag->EmbededDatalen);
	nbytes += tag->EmbededDatalen;
	
//	memset(SockSendBuffer+pos+nbytes, 0, 8); //firstseen_timestamp
//	nbytes += 8;
//	memset(SockSendBuffer+pos+nbytes, 0, 8); //lastseen_timestamp
//	nbytes += 8;
	return nbytes;
#endif
}

void send_evt_synctimereq(void)
{
	if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Http || 
		gRtSetting->upload.sw_potl == Upload_Trans_Potl_Mqtt)
	{
		char *jbuf = (char *)SockSendBuffer;
		AddMsgHeader2SockBuffer_j(jbuf, MidMsgType_SyncTimeReq);
#if Custom_By_Caipan
		sprintf(jbuf+strlen(jbuf), "}}");
#else
		sprintf(jbuf+strlen(jbuf), "{}}");
#endif
		up_send(SockSendBuffer, strlen(jbuf));
	}
}

HeartBeatData_ST gHbData;
uint64 glastpulsetm;
void send_evt_heartbeat(void)
{
	if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Http || 
		gRtSetting->upload.sw_potl == Upload_Trans_Potl_Mqtt)
	{
		char *jbuf = (char *)SockSendBuffer;
		AddMsgHeader2SockBuffer_j(jbuf, MidMsgType_HeartBeat);
#if Custom_By_Caipan
		sprintf(jbuf+strlen(jbuf), "}}");
#else		
		sprintf(jbuf+strlen(jbuf), "%u}", gHbData.hb_count);
#endif
		up_send(SockSendBuffer, strlen(jbuf));
	}
	else
	{
#if Custom_By_CDZNWL
		char *sbuff = (char *)SockSendBuffer;
		int pos = AddMsgHeader2SockBuffer(SockSendBuffer, MidMsgType_HeartBeat, gRdrStateFlag);		
		int i;
		
		sprintf(sbuff+strlen(sbuff), "%02X,%02X,%02X%02X%02X%02X,", 
			gHbData.main_board, gHbData.rfid_mod, gHbData.software_version[0], 
			gHbData.software_version[1], gHbData.software_version[2], 
			gHbData.software_version[3]);

		if (gHbData.antcount == 0)
			strcat(sbuff, "00");
		else
		{
			for (i = 0; i < gHbData.antcount; ++i)
				sprintf(sbuff+strlen(sbuff), "%02X", gHbData.connected_antennas[i]);
		}
		
		sprintf(sbuff+strlen(sbuff), ",%08X,", gHbData.hb_count);
		SetMsgEnd(SockSendBuffer);
		up_send(SockSendBuffer, strlen(sbuff));
#else
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
#endif
	}
	gHbData.hb_count++;
}
/*
void send_pulse(int justpoweron)
{
	unsigned long long now = getSysTick();
	
	if ((now - glastpulsetm) > (gRtSetting->glob_params.hb_cylce*1000) || 
		justpoweron == 1)
	{
		if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Http || 
			gRtSetting->upload.sw_potl == Upload_Trans_Potl_Mqtt)
		{
			char *jbuf = (char *)SockSendBuffer;
			AddMsgHeader2SockBuffer_j(jbuf, MidMsgType_HeartBeat);
			sprintf(jbuf+strlen(jbuf), "%d}", gHbData.hb_count);
			up_send(SockSendBuffer, strlen(jbuf));
		}
		else
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
		}
		
		glastpulsetm = getSysTick();
		gHbData.hb_count++;
	}
}
*/
extern char *gRdrErrStrBuf;
void send_evt_reader_err(void)
{
	uint64 now_64;
	if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Http || 
		gRtSetting->upload.sw_potl == Upload_Trans_Potl_Mqtt)
	{
		char *jbuf = (char *)SockSendBuffer;
		int err_ = 0;
		AddMsgHeader2SockBuffer_j(jbuf, MidMsgType_RdrError);
		now_64 = (uint64)get_utc_secs(getSysTick()/1000)*(uint64)1000;
		if (gRdrStateFlag != 0)
		{
			if (gRdrStateFlag < httpAPIErrCodeBase)
				err_ = gRdrStateFlag+httpAPIErrCodeBase;
		}
		sprintf(jbuf+strlen(jbuf), "{\"err_code\":%d,\"err_string\":\"%s\",\"timestamp\":%lld}}", 
			err_, gRdrErrStrBuf, now_64);
		up_send(SockSendBuffer, strlen(jbuf));
	}
	else
	{
		int pos = AddMsgHeader2SockBuffer(SockSendBuffer, MidMsgType_RdrError, gRdrStateFlag);
		strcpy((char *)(SockSendBuffer+pos), gRdrErrStrBuf);
		if (gCurWorkMode == WorkMode_ActVer_1)
		{
			memset(SockSendBuffer+pos, 0, 24);
			pos += 24;
		}
		else
		{
			strcpy((char *)(SockSendBuffer+pos), gRdrErrStrBuf);
			pos += strlen(gRdrErrStrBuf);
		}
		SetMsgDatalen(SockSendBuffer, pos);
		up_send(SockSendBuffer, pos);
	}
}
/*
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
*/
unsigned char gGpiMap = 0;
int GpiChange(uint8 *gsts)
{
	unsigned char gpimap = 0;
	gpimap |= gpi_get(1);
	gpimap |= (gpi_get(2) << 1);
	gpimap |= (gpi_get(3) << 2);
	gpimap |= (gpi_get(4) << 3);
	
	*gsts = gpimap;
	if (gGpiMap != gpimap)
	{
		gGpiMap = gpimap;
		return 1;
	}
	else
		return 0;
}

void send_evt_gpichan(uint8 state)
{
	if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Http || 
		gRtSetting->upload.sw_potl == Upload_Trans_Potl_Mqtt)
	{
		uint64 now_64;
		int i;
		char *tjson = (char *)SockSendBuffer;
		
		AddMsgHeader2SockBuffer_j(tjson, MidMsgType_GpiTrigger);
		strcat(tjson,"{\"gpi_states\":[");
		for (i = 0; i < 4; ++i)
			sprintf(tjson+strlen(tjson), "{\"gpi\":%d,\"state\":%d},", i+1, (state >> i) & 0x01);
		now_64 = (uint64)get_utc_secs(getSysTick()/1000)*(uint64)1000;
		sprintf(tjson+strlen(tjson)-1, "],\"timestamp\":%lld}}", now_64);
		up_send(SockSendBuffer, strlen(tjson));	
	}
	else
	{
		int pos = AddMsgHeader2SockBuffer(SockSendBuffer, MidMsgType_GpiTrigger, 0);
		SockSendBuffer[pos++] = 4;
		SockSendBuffer[pos++] = 1;
		SockSendBuffer[pos++] = (state >> 0) & 0x01;
		SockSendBuffer[pos++] = 2;
		SockSendBuffer[pos++] = (state >> 1) & 0x01;
		SockSendBuffer[pos++] = 3;
		SockSendBuffer[pos++] = (state >> 2) & 0x01;
		SockSendBuffer[pos++] = 4;
		SockSendBuffer[pos++] = (state >> 3) & 0x01;
		SetMsgDatalen(SockSendBuffer, pos);
		up_send(SockSendBuffer, pos);
	}
}
void send_evt_emptydata()
{
	if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Http || 
		gRtSetting->upload.sw_potl == Upload_Trans_Potl_Mqtt)
	{
		char *tjson = (char *)SockSendBuffer;
					
		AddMsgHeader2SockBuffer_j(tjson, MidMsgType_TagRead);
		strcat(tjson, "]}");
		up_send(SockSendBuffer, strlen(tjson));				
	}
	else
	{
		int pos = AddMsgHeader2SockBuffer(SockSendBuffer, MidMsgType_TagRead, 0);
		AddTagCnt2SockBuffer(SockSendBuffer, 0, pos);
		pos += 2;
		SetMsgDatalen(SockSendBuffer, pos);
		up_send(SockSendBuffer, pos);
	}
}

void send_evt_tagcoming(TAGINFO *tag)
{
	if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Http || 
		gRtSetting->upload.sw_potl == Upload_Trans_Potl_Mqtt)
	{
		char *tjson = (char *)SockSendBuffer;
		int jslen;
					
		AddMsgHeader2SockBuffer_j(tjson, MidMsgType_TagComing);
		AddTag2SockBuffer_j(tjson, tag);
		jslen = strlen(tjson);
		sprintf(tjson+jslen-1, "]}");
		up_send(SockSendBuffer, jslen+1);				
	}
	else
	{
#if Custom_By_HZWXZN
		char *singletag = (char *)SockSendBuffer;
		Hex2Str(tag->EpcId, tag->Epclen, singletag);
		strcat(singletag, ",");
		up_send(SockSendBuffer, strlen(singletag));
#else
		int pos = AddMsgHeader2SockBuffer(SockSendBuffer, MidMsgType_TagComing, 0);
		int tagcntpos = pos;
		pos += 2;
		pos += AddTag2SockBuffer(SockSendBuffer, tag, pos);
		
		AddTagCnt2SockBuffer(SockSendBuffer, 1, tagcntpos);
		SetMsgDatalen(SockSendBuffer, pos);
		up_send(SockSendBuffer, pos);
#endif
	}
}

unsigned char keytable[] = {0x27, 0x1e, 0x1f, 0x20, 0x21, 0x22, 
	0x23, 0x24, 0x25, 0x26, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
unsigned char retr_ = 0x28;
//int gtestsendcnt = 0;
void send_evt_tagbatch(void)
{
	int i;
	int tagcnt;
	int pos = 0;
	int tagcntpos;
//	uint64 lastjstime = 0;
	TAGINFO tmpTag;
	uint8 wgsbuf[8];
	int j;

	//	TRACE("handle App_Evt_BatchMoment %lld\n", getSysTick());
	tagcnt = tagGetCnt();
//	TRACE("---------------------------------------------before GetTagCountInBuffer tagcnt:%d\n", tagcnt);
	if (tagcnt > 0)
	{
		if (gIsEvtTagRead == 0 && gIsSendTagOnly == 0)
		{
			for (i = 0; i < tagcnt; ++i)
				tagGetNext(&tmpTag);
			return;
		}
		if (gRtSetting->upload.hw_inf == Upload_Inf_HidKb)
		{							
			for (i = 0; i < tagcnt; ++i)
			{
				tagGetNext(&tmpTag);
				for (j = 0; j < tmpTag.Epclen; ++j)
				{
					send_key(keytable[(tmpTag.EpcId[j] >> 4) & 0xf], 1);
					send_key(keytable[(tmpTag.EpcId[j] >> 0) & 0xf], 1);
				}
				send_key(retr_, 0);
			}
		}
		else if(gRtSetting->upload.hw_inf == Upload_Inf_Wiegand)
		{
			for (i = 0; i < tagcnt; ++i)
			{
				tagGetNext(&tmpTag);
				if (tmpTag.Epclen < gWgGytes)
					continue;

				for (j = 0; j < gWgGytes; ++j)
				{
					if (gRtSetting->upload.sw_potl_params.wiegand.bytes_order == 0)
						wgsbuf[j] = tmpTag.EpcId[tmpTag.Epclen-gWgGytes+j];
					else
						wgsbuf[j] = tmpTag.EpcId[tmpTag.Epclen-j-1];
				}
				g_wg_send_fn(gRtSetting->upload.sw_potl_params.wiegand.pls_width, 
					gRtSetting->upload.sw_potl_params.wiegand.pls_interval, wgsbuf);
				sleep_ms(gRtSetting->upload.sw_potl_params.wiegand.data_interval);
			}
		}
		else
		{
			/*
			gtestsendcnt++;
			if (gtestsendcnt % 50 == 0)
			{
				uint64 now_ = getSysTick();
				TRACE("sendcnt:%d, now:%lld, dur:%lld, left:%d\n", gtestsendcnt, now_, 
					now_ - lastjstime, get_left_heap_size("ssd"));
				lastjstime = now_;
			}
			*/
			if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Http || 
				gRtSetting->upload.sw_potl == Upload_Trans_Potl_Mqtt)
			{
				char *tjson = (char *)SockSendBuffer;
				int tagbatchcnt = 0;
				int jslen;

				AddMsgHeader2SockBuffer_j(tjson, MidMsgType_TagRead);
				for (i = 0; i < tagcnt; ++i)
				{
					tagGetNext(&tmpTag);
					AddTag2SockBuffer_j(tjson, &tmpTag);
					tagbatchcnt++;
					jslen = strlen(tjson);
					if (strlen(tjson) + 168 >= TagSendBufLen)
					{
#if Custom_By_Caipan
						sprintf(tjson+jslen-1, "],\"tagCount\":%d}}", tagbatchcnt);
						up_send(SockSendBuffer, strlen(tjson));
#elif Custom_By_ZHXX_ZSYH
						sprintf(tjson+jslen-1, "]");
						up_send(SockSendBuffer, strlen(tjson));
#else
						sprintf(tjson+jslen-1, "]}");
						up_send(SockSendBuffer, jslen+1);
#endif
						//TRACE("+++++++++++++++++++++ up_send tagbatchcnt:%d, size:%d\n", tagbatchcnt, jslen+1);					
						AddMsgHeader2SockBuffer_j(tjson, MidMsgType_TagRead);
						tagbatchcnt = 0;
					}
				}
				if (tagbatchcnt != 0)
				{
					jslen = strlen(tjson);
#if Custom_By_Caipan					
					sprintf(tjson+jslen-1, "],\"tagCount\":%d}}", tagbatchcnt);
					up_send(SockSendBuffer, strlen(tjson));
#elif Custom_By_ZHXX_ZSYH
					sprintf(tjson+jslen-1, "]");
					up_send(SockSendBuffer, strlen(tjson));
#else
					sprintf(tjson+jslen-1, "]}");
					up_send(SockSendBuffer, jslen+1);
#endif					
					//TRACE("+++++++++++++++++++++++ last up_send tagbatchcnt:%d, size:%d\n", tagbatchcnt, jslen+1);					
				}					
			}
			else
			{
				int tagbatchcnt = 0;
				pos = AddMsgHeader2SockBuffer(SockSendBuffer, MidMsgType_TagRead, 0);
				tagcntpos = pos;
				pos += 2;
				for (i = 0; i < tagcnt; ++i)
				{
					tagGetNext(&tmpTag);
					pos += AddTag2SockBuffer(SockSendBuffer, &tmpTag, pos);
					tagbatchcnt++;
#if Custom_By_CDZNWL
					if (strlen((char *)SockSendBuffer) + gPRdrStaSet->app_init.max_tb_rec_len*2 + 60 >= TagSendBufLen)
#elif Custom_By_GZTD || Custom_By_HZWXZN
					if (strlen((char *)SockSendBuffer) + gPRdrStaSet->app_init.max_tb_rec_len*2 + 100 >= TagSendBufLen)
#else
					if (pos + gPRdrStaSet->app_init.max_tb_rec_len + 30 >= TagSendBufLen)
#endif
					{
#if Custom_By_CDZNWL
						SetMsgEnd(SockSendBuffer);
						up_send(SockSendBuffer, strlen((char *)SockSendBuffer));
#else
						AddTagCnt2SockBuffer(SockSendBuffer, tagbatchcnt, tagcntpos);
						SetMsgDatalen(SockSendBuffer, pos);
#if Custom_By_GZTD || Custom_By_HZWXZN					
						up_send(SockSendBuffer, strlen((char *)SockSendBuffer));						
#else
						up_send(SockSendBuffer, pos);
#endif
#endif
						//					TRACE("up_send tagbatchcnt:%d, size:%d\n", tagbatchcnt, pos);
						
						pos = AddMsgHeader2SockBuffer(SockSendBuffer, MidMsgType_TagRead, 0);
						tagbatchcnt = 0;
						pos = tagcntpos + 2;
					}
				}
				if (tagbatchcnt != 0)
				{
#if Custom_By_CDZNWL
					SetMsgEnd(SockSendBuffer);
					up_send(SockSendBuffer, strlen((char *)SockSendBuffer));
#else
					AddTagCnt2SockBuffer(SockSendBuffer, tagbatchcnt, tagcntpos);
					SetMsgDatalen(SockSendBuffer, pos);
#if Custom_By_GZTD || Custom_By_HZWXZN
					up_send(SockSendBuffer, strlen((char *)SockSendBuffer));
#else
					up_send(SockSendBuffer, pos);
#endif
#endif
					//				TRACE("last up_send tagbatchcnt:%d, size:%d\n", tagbatchcnt, pos);			
				}
			}
		}
	}
	else
	{
		if (gIsEvtEmptyData == 1)
			send_evt_emptydata();
	}
}

/*
void gpiChecker(int flag)
{
	unsigned char gpimap = 0;
	gpimap |= gpi_get(1);
	gpimap |= (gpi_get(2) << 1);
	gpimap |= (gpi_get(3) << 2);
	gpimap |= (gpi_get(4) << 3);
	if (flag == 1)
	{
		if (gGpiMap != gpimap)
		{
			int pos = AddMsgHeader2SockBuffer(SockSendBuffer, MidMsgType_GpiTrigger, 0);
			SockSendBuffer[pos++] = 4;
			SockSendBuffer[pos++] = 1;
			SockSendBuffer[pos++] = (gpimap >> 0) & 0x01;
			SockSendBuffer[pos++] = 2;
			SockSendBuffer[pos++] = (gpimap >> 1) & 0x01;
			SockSendBuffer[pos++] = 3;
			SockSendBuffer[pos++] = (gpimap >> 2) & 0x01;
			SockSendBuffer[pos++] = 4;
			SockSendBuffer[pos++] = (gpimap >> 3) & 0x01;
			
			SetMsgDatalen(SockSendBuffer, pos);
			up_send(SockSendBuffer, pos);
		}
	}
	
	gGpiMap = gpimap;
}
*/
int RecvCmd(int rtimeout, int *fd, int *cmdid, int *datalen, int *iscrc)
{
	int commonfd = -1;
	int socks[1];
	int *sokpara = NULL;
	int uarts[] = {COMMON_INTERFACE_UART2, COMMON_INTERFACE_UART3, COMMON_INTERFACE_UART1};
	int ret;
	int uartcnt = 2;
	int pos = 0;
//	int i;
	
	if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Tcp)
	{
		if (gRtSetting->upload.hw_inf == Upload_Inf_Ethernet)
		{
			socks[0] = COMMON_INTERFACE_SOCKET0;
			sokpara = socks;
		}
		else if (gRtSetting->upload.hw_inf == Upload_Inf_4G || 
			gRtSetting->upload.hw_inf == Upload_Inf_Wifi)
			uartcnt++;
	}
	
//	for (i = 0; i < rtimeout / 5; ++i)
//	{
		commonfd = apt_multi_infs_select_nob(NULL, uarts, uartcnt, sokpara, 1);
//		if (commonfd >= 0)
//			break;
//		sleep_ms(5);
//	}
	
	if (commonfd < 0)
		return -1;
	
	
	ret = read(commonfd, SockRecvBuffer+pos, 1);
	if (ret != 1)
	{
//		printf("recv first byte ret:%d\n", ret);
		return -1;
	}
	ioctl(commonfd, COMMON_INTERFACE_SET_TIMEOUT, &rtimeout);
	
	pos++;
	if (SockRecvBuffer[0] != 0xEE)
	{
		TRACE("111111111111 (SockRecvBuffer[0] != 0xEE), %02X, commonfd:%d\n", 
			SockRecvBuffer[0], commonfd);
		return -1;
	}
	ret = read_n(commonfd, SockRecvBuffer+pos, 5);
	if (ret != 5)
	{
		TRACE("recv header failed\n");
		return -1;
	}
	
	
	pos += 5;

	*datalen = (SockRecvBuffer[2] << 8) | SockRecvBuffer[3];
	*cmdid = SockRecvBuffer[4];
	*iscrc = (SockRecvBuffer[5] >> 1) & 0x01;
	*fd = commonfd;
//	TRACE("cmdid:%d, datalen:%d\n", *cmdid, *datalen);
	return 0;
}

void RemoteCmd(void)
{
	int datalen;
	int cmdid;
	int iscrc;
	int fd;
	if (RecvCmd(20, &fd, &cmdid, &datalen, &iscrc) == 0)
		custom_ee_commond(fd, SockRecvBuffer, 3, NULL, EEcmd_reply, crc_Msg, gRtSetting);
}

