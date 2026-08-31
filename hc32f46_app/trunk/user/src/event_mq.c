#include <string.h>
#include "app_conf.h"
#include "event_mq.h"
#include "reader_msg.h"

typedef struct
{
	uint8 eid;
	uint8 ant;
	uint8 rd;
	uint8 rssi;
	uint8 potl;
	uint8 epclen;
	uint8 bklen;
	uint8 e_b_index;
	int fre;
	uint8 res[4];
	uint32 tm;
} MsgQueObj_ST;

typedef struct
{
	uint8 *buffer;
	uint8 *is_use;
} MsgQueShareMem_ST;
MsgQueShareMem_ST gMQShMem;

osMessageQueueId_t gIdMsgQue;
uint64 gHbLastTime = 0;
uint64 gBatchLastTime = 0;
uint8 gIsEvtGpiChan = 0;
uint8 gIsEvtEmptyData = 0;
uint8 gIsEvtHeartbeat = 1;
uint8 gIsEvtTagComing = 1;
uint8 gIsSendTagOnly = 0;
uint8 gIsEvtTagRead = 1;
uint8 gIsEvtSyncTimeReq = 0;
uint8 gIsSendRdrErr = 1;

int EvtContains(rdr_rt_evt_code ecode)
{
	int i;
	for (i = 0; i < gRtSetting->events.count; ++i)
	{
		if (gRtSetting->events.ids[i] == ecode)
			return 1;
	}
	
	return 0;
}

void init_evt_sys(void)
{
	osMessageQueueAttr_t mqAttr;
	int mqdsize;
	int i;
	////
	mqdsize = sizeof(MsgQueObj_ST);
	mqdsize += 32 - mqdsize %32;
	mqdsize = mqdsize * gPRdrStaSet->app_init.evt_que_len;
	TRACE("mqdsize:%d\n", mqdsize);
	
	mqAttr.name = NULL;
	mqAttr.attr_bits = 0;
	mqAttr.cb_mem = malloc_hexp(sizeof(osRtxMessageQueue_t));
	mqAttr.cb_size = sizeof(osRtxMessageQueue_t);
	mqAttr.mq_mem = malloc_hexp(mqdsize);
	mqAttr.mq_size = mqdsize;

	gIdMsgQue = osMessageQueueNew(gPRdrStaSet->app_init.evt_que_len, sizeof(MsgQueObj_ST), &mqAttr);
	if (gIdMsgQue != NULL)
		TRACE("osMessageQueueNew is ok\n");
	else
		TRACE("osMessageQueueNew is failed\n");
	gMQShMem.buffer = malloc_hexp(gPRdrStaSet->app_init.max_tb_rec_len * 
		gPRdrStaSet->app_init.evt_que_len);
	gMQShMem.is_use = malloc_hexp(gPRdrStaSet->app_init.evt_que_len);
	for (i = 0; i < gPRdrStaSet->app_init.evt_que_len; ++i)
		gMQShMem.is_use[i] = 0;
	
	gIsEvtTagRead = EvtContains(rdr_rt_evt_TagRead);
	gIsEvtGpiChan = EvtContains(rdr_rt_evt_GpiChange);
	gIsEvtEmptyData = EvtContains(rdr_rt_evt_EmptyData);
	gIsEvtHeartbeat = EvtContains(rdr_rt_evt_HeartBeat);
	gIsEvtTagComing = EvtContains(rdr_rt_evt_TagComing);
	gIsEvtSyncTimeReq = EvtContains(rdr_rt_evt_SyncTimeReq);
	
#if Custom_By_Caipan
	gIsEvtEmptyData = 0;
	gIsEvtTagComing = 0;
	gIsSendRdrErr = 0;
	gIsEvtGpiChan = 0;
	gIsEvtSyncTimeReq = 1;
#elif Custom_By_ZHXX_ZSYH
	gIsEvtEmptyData = 0;
	gIsEvtTagComing = 0;
	gIsSendRdrErr = 0;
	gIsEvtGpiChan = 0;
	gIsEvtSyncTimeReq = 0;
	gIsEvtHeartbeat = 0;
#elif Custom_By_CDZNWL
	if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Tcp || 
		gRtSetting->upload.hw_inf == Upload_Inf_Uart_1 || 
		gRtSetting->upload.hw_inf == Upload_Inf_Uart_2)
	{
		gIsEvtEmptyData = 0;
		gIsEvtTagComing = 0;
		gIsSendRdrErr = 0;
		gIsEvtGpiChan = 0;
		gIsEvtSyncTimeReq = 0;
		gIsEvtHeartbeat = 1;
		gRtSetting->upload.client_ack = 0;
		gRtSetting->upload.crc_enable = 0;
	}
#elif Custom_By_GZTD || Custom_By_HZWXZN
	if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Tcp || 
		gRtSetting->upload.hw_inf == Upload_Inf_Uart_1 || 
		gRtSetting->upload.hw_inf == Upload_Inf_Uart_2)
	{
		gIsEvtEmptyData = 0;
#if Custom_By_GZTD
		gIsEvtTagComing = 0;
#endif
		gIsSendRdrErr = 0;
		gIsEvtGpiChan = 0;
		gIsEvtSyncTimeReq = 0;
		gIsEvtHeartbeat = 0;
		gRtSetting->upload.client_ack = 0;
		gRtSetting->upload.crc_enable = 0;
	}
#endif
	if (gRtSetting->upload.hw_inf == Upload_Inf_HidKb || 
		gRtSetting->upload.hw_inf == Upload_Inf_Wiegand)
		gIsSendTagOnly = 1;
	
	if (gIsEvtGpiChan ==1)
	{
		gGpiMap = 0;
		gGpiMap |= gpi_get(1);
		gGpiMap |= (gpi_get(2) << 1);
		gGpiMap |= (gpi_get(3) << 2);
		gGpiMap |= (gpi_get(4) << 3);
	}
	TRACE("gIsEvtTagRead:%d,gIsEvtGpiChan:%d,gIsEvtEmptyData:%d,gIsEvtHeartbeat:%d,gIsEvtTagComing:%d,gIsEvtSyncTimeReq:%d,gIsSendTagOnly:%d\n",
		gIsEvtTagRead, gIsEvtGpiChan, gIsEvtEmptyData, gIsEvtHeartbeat, gIsEvtTagComing, gIsEvtSyncTimeReq, gIsSendTagOnly);
}

int put_evt_que(TAGINFO *evt)
{
	MsgQueObj_ST mqObj;
	int i;
	uint8 *cppos;
	osStatus_t osSta;
	int isfind = 0;
	int eqlen = gPRdrStaSet->app_init.evt_que_len;
	
	mqObj.eid = evt->PC[0];
	if (mqObj.eid == App_Evt_TagComing)
	{
		mqObj.ant = evt->AntennaID;
		mqObj.rd = evt->ReadCnt;
		mqObj.potl = evt->protocol;
		mqObj.rssi = (uint8)evt->RSSI;
		mqObj.epclen = evt->Epclen;
		mqObj.fre = evt->Frequency;
		mqObj.tm = evt->TimeStamp;
		memcpy(mqObj.res, evt->Res, 2);
		memcpy(mqObj.res+2, evt->CRC, 2);
		for (i = 0; i < eqlen; ++i)
		{
			if (gMQShMem.is_use[i] == 0)
			{
				isfind = 1;
				break;
			}
		}
		if (isfind == 0)
		{
			TRACE("put_evt_que not find slot\n");
			return -1;
		}
		gMQShMem.is_use[i] = 1;
		mqObj.e_b_index = i;
		cppos = gMQShMem.buffer+i*gPRdrStaSet->app_init.max_tb_rec_len;
		memcpy(cppos, evt->EpcId, mqObj.epclen);
		mqObj.bklen = evt->EmbededDatalen;
		if (mqObj.bklen > 0)
			memcpy(cppos+mqObj.epclen, evt->EmbededData, mqObj.bklen);
	}
	else if (mqObj.eid == App_Evt_GpiChange)
		mqObj.rssi = evt->PC[1];

	osSta = osMessageQueuePut(gIdMsgQue, &mqObj, NULL, NULL);
	if (osSta != osOK)
		return -2;
	
	return 0;
}

int get_evt_que(TAGINFO *evt)
{
	osStatus_t gmqret;
	MsgQueObj_ST mqObj;
	uint8 *cppos;
	
	gmqret = osMessageQueueGet(gIdMsgQue, &mqObj, NULL, NULL);
	if (gmqret == osOK)
	{
		evt->PC[0] = mqObj.eid;
		if (mqObj.eid == App_Evt_TagComing)
		{
			evt->AntennaID = mqObj.ant;
			evt->Epclen = mqObj.epclen;
			evt->ReadCnt = mqObj.rd;
			evt->TimeStamp = mqObj.tm;
			evt->Phase = mqObj.tm;
			memcpy(evt->Res, mqObj.res, 2);
			memcpy(evt->CRC, mqObj.res+2, 2);
			evt->Frequency = mqObj.fre;
			cppos = gMQShMem.buffer+mqObj.e_b_index*gPRdrStaSet->app_init.max_tb_rec_len;
			memcpy(evt->EpcId, cppos, mqObj.epclen);
			evt->EmbededDatalen = mqObj.bklen;
			if (mqObj.bklen > 0)
				memcpy(evt->EmbededData, cppos+mqObj.epclen, mqObj.bklen);
			evt->RSSI = (signed char)mqObj.rssi;
			evt->protocol = (SL_TagProtocol)mqObj.potl;
			gMQShMem.is_use[mqObj.e_b_index] =0;
		}
		else if (mqObj.eid == App_Evt_GpiChange)
			evt->PC[1] = mqObj.rssi;
		
		return 0;
	}
	return -1;
}






