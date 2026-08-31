#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ModuleReader.h"
#include "hc32f46_driver.h"
#include "mp_pool.h"
#include "app_conf.h"
#include "http_callback.h"
#include "mqtt_interface.h"
#include "MQTTClient.h"
#include "reader_msg.h"
#include "reader_init.h"
#include "ErrChecker.h"
#include "event_mq.h"


int user_main_thstk_size = 1024*4;
osPriority_t user_main_priority = osPriorityNormal;
int is_enable_fwupdate = 2;

int reason2 = 0;
int reason1 = 0;

READER_ERR gRdrErr = MT_OK_ERR;
volatile int gErrSend = 0;
volatile int gRdrStateFlag = 0;



WorkMode_Code gCurWorkMode = WorkMode_None;
volatile int gIsFinInit = 0;

ReaderRunTimeSettings_ST *gRtSetting = NULL;
ReaderStaticSettings_ST *gPRdrStaSet = NULL;
int gIsStartAsyncInv = 0;
void wait_init_ok(void)
{
	while (1)
	{
		if (gIsFinInit == 1)
			break;
		sleep_ms(50);
	}
}

typedef struct  
{
	volatile int finflag[4];
	unsigned long long starttime;
	volatile int isFire;
} LocGpoSet_ST;
LocGpoSet_ST gTRLocGpoActs;

void resetLocGpoSet(LocGpoSet_ST *pLGS)
{
	int i;
	pLGS->isFire = 0;
	for (i = 0; i < 4; ++i)
		pLGS->finflag[i] = 0;
}

void checkLocGPOAct(void *data)
{
	LocGpoSet_ST *pLGS = (LocGpoSet_ST *)data;
	if (gRtSetting->gpo_act.count > 0 && pLGS->isFire == 1)
	{
		int i;
		int isallfin = 1;
		int dur = getSysTick() - pLGS->starttime;
		
		for (i = 0; i < gRtSetting->gpo_act.count; ++i)
		{
			if (pLGS->finflag[i] == 0)
			{
				if (dur >= gRtSetting->gpo_act.durs[i])
				{
					gpo_set(gRtSetting->gpo_act.ids[i], 1 - gRtSetting->gpo_act.states[i]);
					pLGS->finflag[i] = 1;
				}
				isallfin = 0;
			}
		}
		if (isallfin == 1)
			resetLocGpoSet(pLGS);
	}
}

void send_tags(void *arg)
{
	TAGINFO tmpTag;
	uint64 now_time;
	
	wait_init_ok();
	if (gIsSendTagOnly == 0)
	{
		if (gIsEvtHeartbeat == 1)
		{
			send_evt_heartbeat();
			gHbLastTime = getSysTick(); 
		}
		if (gIsEvtSyncTimeReq == 1 && 
			(gRtSetting->upload.sw_potl == Upload_Trans_Potl_Http || 
			gRtSetting->upload.sw_potl == Upload_Trans_Potl_Mqtt))
		{
			while (1)
			{
				TRACE("send synctimereq\n");
				send_evt_synctimereq();
				if (gUtcSecBase > 0)
					break;
				sleep_ms(2000);
			}
		}
	}
	gBatchLastTime = getSysTick();
	
	while (1)
	{
		if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Mqtt)
			mqtt_task(NULL, 0);
		RemoteCmd();
		sleep_ms(5);
		//////////////////
		now_time = getSysTick();
		
		if (gIsSendTagOnly == 0)
		{
			if (gIsEvtHeartbeat == 1)
			{
				if (now_time - gHbLastTime >= gRtSetting->glob_params.hb_cylce*1000)
				{
					send_evt_heartbeat();
					gHbLastTime = getSysTick();		
				}
			}
			if (gIsEvtGpiChan == 1)
			{
				uint8 gpistates;
				if (GpiChange(&gpistates) == 1)
					send_evt_gpichan(gpistates);
			}
			if (gIsSendRdrErr == 1)
			{
				if (gErrSend != 0)
				{
					gRdrStateFlag = gErrSend;
					send_evt_reader_err();
					gErrSend = 0;
				}
			}
		}
		
		if (gRtSetting->gpi_trigger.is_gpi_trigger != 1 && 
			gRtSetting->upload.data_aggr.mode == 1 && gRdrStateFlag == 0)
		{
			if (now_time - gBatchLastTime >= gRtSetting->upload.data_aggr.timeval)
			{
				send_evt_tagbatch();
				gBatchLastTime = now_time;
			}
		}
		
		//////////////////
		if (get_evt_que(&tmpTag) == 0) 
		{
			switch(tmpTag.PC[0])
			{
				case App_Evt_TagComing:
					if (gIsSendTagOnly == 0)
						send_evt_tagcoming(&tmpTag);
					break;
				case App_Evt_BatchMoment:
				{
					send_evt_tagbatch();
					break;
				}
				default:
					break;
			}
		}
	}
}

typedef enum
{
		BackReadGpi_WaitStart = 0,
		BackReadGpi_WaitStop = 1,
		BackReadGpi_WaitTimeout = 2,
} BackReadGpiTriState;
void getAllGpi(rdr_rt_set_gpi_cond *pGinfo)
{
	pGinfo->count = 4;
	pGinfo->ids[0] = 1;
	pGinfo->states[0] = gpi_get(1);
	pGinfo->ids[1] = 2;
	pGinfo->states[1] = gpi_get(2);
	pGinfo->ids[2] = 3;
	pGinfo->states[2] = gpi_get(3);
	pGinfo->ids[3] = 4;
	pGinfo->states[3] = gpi_get(4);
}

int GpiTriContains(rdr_rt_set_gpi_cond *littleGInfo, rdr_rt_set_gpi_cond *bigGInfo)
{
	int i;
	for (i = 0; i < littleGInfo->count; ++i)
	{
		if (bigGInfo->states[littleGInfo->ids[i] - 1] != littleGInfo->states[i])
			return 0;
	}
	return 1;
}

void fireLocGPOAct(LocGpoSet_ST *pLGS)
{
#if Custom_By_Caipan || Custom_By_ZHXX_ZSYH
#else
	if (gRtSetting->gpo_act.count > 0 && pLGS->isFire == 0)
	{
		int i;
		for (i = 0; i < gRtSetting->gpo_act.count; ++i)
			gpo_set(gRtSetting->gpo_act.ids[i], gRtSetting->gpo_act.states[i]);
		
		pLGS->starttime = getSysTick();
		pLGS->isFire = 1;
	}
#endif
}

void tagInsert_wp(READER_ERR gErr, TAGINFO *tag, int triid, LocGpoSet_ST * pLGS)
{
	int Repet;
	
	if (gErr == MT_OK_ERR)
	{
		if (tag->Epclen+tag->EmbededDatalen <= gPRdrStaSet->app_init.max_tb_rec_len)
		{
			if (gRtSetting->gpi_trigger.is_gpi_trigger == 1)
			{
				if (gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1ORTRI2START_TRI1ORTRI2STOP || 
					gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1ORTRI2START_TIMEOUTSTOP) 
				{
					if (gRtSetting->gpi_trigger.cond_order == 1)
						tag->protocol = (SL_TagProtocol)triid;
					else
						tag->protocol = (SL_TagProtocol)(3 - triid);
				}						
			}

			tag->TimeStamp = getSysTick()/1000;
			Repet = tagInsert(tag);
			if (Repet == -1)
			{
				TAGINFO tmpTag;
				tagGetNext(&tmpTag);
				Repet = tagInsert(tag);
//				TRACE("tag buffer is full 99999999999999999999999999999999999999999999999\n");
			}
			if (gIsEvtTagComing == 1 && Repet == 0)
			{
				tag->Phase = tag->TimeStamp;
				tag->PC[0] = App_Evt_TagComing;
				put_evt_que(tag);
			}
			fireLocGPOAct(pLGS);
		}
	}
}

const int DefNoSndGPOVal = 0;
//#define TagBufferMemSize (1024*48)
//int init_mbedtls(void);
char *gRdrErrStrBuf;
const uint8 GPITRIGGER_TRI1ANDTRI2START_TIMEOUTSTOP = 5;

void user_main_active(void)
{
//	baseParaConfig config;
	int tagcnt;
	int i;
	TAGINFO tag;
	osThreadAttr_t thAttr_t;
//	int tb_conf_is = 0;
	////gpi trigger
	BackReadGpiTriState BRGstate = BackReadGpi_WaitStart;
	rdr_rt_set_gpi_cond gstates;
	int laststopgpitriid = -1;
	int CanStartTRiStopCase = 1;
	unsigned long long CanStartTm1;
	unsigned long long CanStartTm2;
	unsigned long long dtwstart;
	unsigned long long timenow;
	int startgpitriid = 0;
	int isGpiTriStop = 0;
	
	init_evt_sys();
	//////// set local gpo
//	int IsGpoSet = 0;
//	unsigned long long LastGpoSetTime;
	////////
//	gLastGetTemperTime= getSysTick();
//	gInitGetTemperTime= gLastGetTemperTime;

//	gRemoteGPOSet.isRun = 0;
//	led_on();
	/*
	for (i = 0; i < 4; ++i)
	{
		if (gRsSetting->gpos[i] != 0)
			gpo_set(i+1, gRsSetting->gpos[i]-1);
		else
			gpo_set(i+1, DefNoSndGPOVal);
	}
	*/	
	echr_init(90);
	gRtSetting->cus_param.param[gRtSetting->cus_param.len] = 0;
	TRACE("gRtSetting->cus_param.param:%s\n", gRtSetting->cus_param.param);
	
	resetLocGpoSet(&gTRLocGpoActs);
	add_cycle_task(checkLocGPOAct, &gTRLocGpoActs);
	dump_runtime_settings(gRtSetting);
	
//	printf("enter  main 10.0.0.15.011\n");
	////////
	init_osThreadAttr_t(&thAttr_t, 1024*4, osPriorityNormal);
	osThreadNew(send_tags, NULL, &thAttr_t);
	/////////
//	init_osThreadAttr_t(&thAttr_t, 512*3, osPriorityNormal);
//	osThreadNew(event_generator, NULL, &thAttr_t);
	
	init_usb(1);
	sleep_ms(100);
//	if (gRtSetting->upload.inf_mode == Upload_Inf_Uart)
//		InitDegutPrintf(2, COMMON_INTERFACE_SOCKET2, "192.168.1.44", 9999);
	if (init_upload() != 0)
		goto FIN;
	
	for (i = 0; i < 3; ++i)
	{
//		printf("reconf:%d\n", reconf);
		gRdrErr = OpenReader();
		TRACE("------ gRdrErr:%d\n", gRdrErr);
		if (gRdrErr == MT_OK_ERR)
			break;
		else if (gRdrErr == MT_HARDWARE_ALERT_ERR_BY_NO_ANTENNAS)
			led_toggle(-1, 1000, NULL);
		else
			led_toggle(-1, 200, NULL);
	}
	
	get_left_heap_size("after OpenReader");
	gIsFinInit = 1;
	
	if (gRdrErr != MT_OK_ERR)
	{
		reason1 = 1;
		gErrSend = (int)gRdrErr;
		goto FIN;
	}

	TRACE("InitReader ok\n");
//	heaptop = malloc(4);
//	printf("4444444444444444444 heaptop:%p\n", heaptop);
	
	TRACE("cycle:%d, gCanAsyncInv:%d\n", gPRdrStaSet->tagops_param.inventory.cycle, gCanAsyncInv);
	
	TRACE("Reader initialization is finished\n");
	
	led_on();
	//////gpi trigger
	if (gRtSetting->gpi_trigger.is_gpi_trigger == 1)
	{
		if (gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1START_TRI2STOP || 
			gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1ORTRI2START_TRI1ORTRI2STOP)
			isGpiTriStop = 1;
	}
	//////
	while (1)
	{
//		int gettagcnt = 1;
		if (gRtSetting->gpi_trigger.is_gpi_trigger == 1)
		{
			int isStart = 1;
			int isStop = 1;
			if (BRGstate == BackReadGpi_WaitStart)
			{
				getAllGpi(&gstates);
				if (laststopgpitriid != -1 && 
					(gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1ORTRI2START_TRI1ORTRI2STOP || 
						gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1START_TRI2STOP))
				{
					if (CanStartTRiStopCase == 0)
					{
						int isLastCondOn;
						if (laststopgpitriid == 1)
							isLastCondOn = GpiTriContains(&gRtSetting->gpi_trigger.cond_1, &gstates);
						else
							isLastCondOn = GpiTriContains(&gRtSetting->gpi_trigger.cond_2, &gstates);
						if (isLastCondOn == 1)
							CanStartTm1 = getSysTick();
						else
						{
							CanStartTm2 = getSysTick();
							if ((CanStartTm2 - CanStartTm1) >= gRtSetting->gpi_trigger.timeval)
								CanStartTRiStopCase = 1;							
						}
						if (CanStartTRiStopCase == 0)
						{
							sleep_ms(20);
							continue;
						}
					}
				}
				
				isStart = GpiTriContains(&gRtSetting->gpi_trigger.cond_1, &gstates);
				if (isStart == 1)
					startgpitriid = 1;
				else if (gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1ORTRI2START_TIMEOUTSTOP || 
					gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1ORTRI2START_TRI1ORTRI2STOP)
				{
					isStart = GpiTriContains(&gRtSetting->gpi_trigger.cond_2, &gstates);
					if (isStart == 1)
						startgpitriid = 2;
				}
				
				if (isStart == 1)
				{
					if (isGpiTriStop == 1)
						BRGstate = BackReadGpi_WaitStop;
					else
					{
						dtwstart = getSysTick();
						BRGstate = BackReadGpi_WaitTimeout;
					}
					if (gPRdrStaSet->tagops_param.inventory.cycle == 0 && gCanAsyncInv == 1)
						gIsStartAsyncInv = 0;
				}
				else
				{
					sleep_ms(10);
					continue;
				}				
			}
			else
			{
				if (BRGstate == BackReadGpi_WaitStop)
				{
					getAllGpi(&gstates);
					if (startgpitriid == 1)
						isStop = GpiTriContains(&gRtSetting->gpi_trigger.cond_2, &gstates);
					else
						isStop = GpiTriContains(&gRtSetting->gpi_trigger.cond_1, &gstates);
						
					if (isStop == 1)
					{
						BRGstate = BackReadGpi_WaitStart;
						if (gPRdrStaSet->tagops_param.inventory.cycle == 0 && gCanAsyncInv == 1)
						{							
							gRdrErr = SyncStopFastReading(ghReader);
							if (gRdrErr != MT_OK_ERR)
							{
								if (HandleModErr() != 0)
								{
									reason1 = 3;
									goto FIN;
								}
							}
						}
						////insert event
						tag.PC[0] = App_Evt_BatchMoment;
						put_evt_que(&tag);
						/////
						laststopgpitriid = 3-startgpitriid;
						CanStartTRiStopCase = 0;
						CanStartTm1 = getSysTick();
						continue;
					}
				}
				else if (BRGstate == BackReadGpi_WaitTimeout)
				{
					timenow = getSysTick();
					if ((timenow - dtwstart) > (gRtSetting->gpi_trigger.timeval))
					{
						BRGstate = BackReadGpi_WaitStart;
						if (gPRdrStaSet->tagops_param.inventory.cycle == 0 && gCanAsyncInv == 1)
						{
							gRdrErr = SyncStopFastReading(ghReader);						
							if (gRdrErr != MT_OK_ERR)
							{
								if (HandleModErr() != 0)
								{
									reason1 = 3;
									goto FIN;
								}
							}
						}
						////insert event
						tag.PC[0] = App_Evt_BatchMoment;
						put_evt_que(&tag);
						/////	
						continue;
					}
				}
			}
		}
		
		if (gPRdrStaSet->tagops_param.inventory.cycle == 0 && gCanAsyncInv == 1)
		{
			if (gIsStartAsyncInv == 0)
			{
				TRACE("SyncStartFastReading\n");
				gRdrErr = SyncStartFastReading(ghReader, gConnants.connectedants, gConnants.antcnt, 
					(((0x1 << 1) | (0x1 << 2) | (0x1 << 3) | (0x1 << 7)) << 8) | 0x80 | (0x01 << 24));
				gIsStartAsyncInv = 1;
			}
		}
		else
		{
			if (gRdrErr == MT_OK_ERR)
			{
				if (gPRdrStaSet->tagops_param.inventory.cycle == 0)
					gPRdrStaSet->tagops_param.inventory.cycle = 150;
//				TRACE("44444444444444444444\n");
				gRdrErr = TagInventory_Raw(ghReader, gConnants.connectedants, gConnants.antcnt, 
					gPRdrStaSet->tagops_param.inventory.cycle*gConnants.antcnt, &tagcnt);
			}
		}
		
		if (gRdrErr == MT_OK_ERR || gRdrErr == MT_CMD_NO_TAG_ERR)
		{
			if (gPRdrStaSet->tagops_param.inventory.cycle == 0 && gCanAsyncInv == 1)
			{
				if (gRdrErr == MT_OK_ERR || gRdrErr == MT_CMD_NO_TAG_ERR)
				{
//					printf("222222222222222222\n");
					gRdrErr = SyncGetNextTag(ghReader, &tag);
					tagInsert_wp(gRdrErr, &tag, startgpitriid, &gTRLocGpoActs);
				}
			}
			else
			{
				for (i = 0; i < tagcnt; ++i)
				{
					gRdrErr = GetNextTag(ghReader, &tag);
					tagInsert_wp(gRdrErr, &tag, startgpitriid, &gTRLocGpoActs);
					if (gRdrErr != MT_OK_ERR)
						break;				
				}
			}
//			printf("GetNextTag err:%d\n", gRdrErr);				
			if (!(gRdrErr == MT_CMD_NO_TAG_ERR || gRdrErr == MT_OK_ERR))
			{
				TRACE("GetNextTag err:%d\n", gRdrErr);
				if (HandleModErr() != 0)
				{
					reason1 = 3;
					goto FIN;
				}
			}
			
			if (gPRdrStaSet->tagops_param.inventory.cycle != 0)
				sleep_ms(gPRdrStaSet->tagops_param.inventory.interval);
		}
		else
		{
			TRACE("TagInventory err:%d\n", gRdrErr);
			if (HandleModErr() != 0)
			{
				reason1 = 2;
				goto FIN;
			}
		}
		
	}
				
FIN:	
	led_off();
	while(1)
	{
		sleep_ms(1000);
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
			printf("user_main Unknown reason\n");
		*/
	}
	
//	sleep_ms(2000);
	
//	system_reset();	
	/*
	while(1)
	{
		sleep_ms(1000);
	}
	*/
}

void user_main_passive(void);
extern volatile int uart0fd;
int user_main_httpapi(void);
/*
#define BUFFER_SIZE	2048
unsigned char tempBuffer[BUFFER_SIZE] = {0};
unsigned char targetIP[4] = {192,168,1,44}; // mqtt server IP
unsigned int targetPort = 8883; // mqtt server port

struct opts_struct
{
	char* clientid;
	int nodelimiter;
	char* delimiter;
	enum QoS qos;
	char* username;
	char* password;
	char* host;
	int port;
	int showtopics;
} opts ={ (char*)"stdout-subscriber", 0, (char*)"\n", QOS0, NULL, NULL, (char *)targetIP, 1883, 0 };
const char MQTT_TOPIC[]="W6100";
*/


void user_main(void)
{
	int i;
	int ispassive = 1;
	
	wait_fin_init();
	brdcst_conf_init(getMaxSocketId());
	gCurWorkMode = TestFwType_ex();
	mp_init(JsonParseMemSize);
	
#ifdef _DEBUG
#if (AppDubugPrintf == 1)
	InitDegutPrintf(1, COMMON_INTERFACE_SOCKET2, "192.168.1.44", 9999);
#elif (AppDubugPrintf == 2)
	InitDegutPrintf(2, getMaxSocketId(), "192.168.1.44", 9999);
#endif
#endif

	gPRdrStaSet = malloc_hexp(sizeof(ReaderStaticSettings_ST));
	get_rdr_static_settings(gPRdrStaSet);
	gRdrErrStrBuf = malloc_hexp(100);
	TRACE("user_main gCurWorkMode:%d\n", gCurWorkMode);
	/*
	{
		uint8 tessdat[24];
		int i;
		int ret;
		int scount = 0;
		
		gSerIp[0] = 192;
		gSerIp[1] = 168;
		gSerIp[2] = 1;
		gSerIp[3] = 44;
		gSerPort = 12345;
		for (i = 0; i < 24; ++i)
			tessdat[i] = i;
		
		CheckServerConnection();
		while(1)
		{
			if (write(COMMON_INTERFACE_SOCKET0, tessdat, 24) != 24)
				break;
			else
			{
				scount++;
				if (scount == 10)
					break;
				printf("send successfully\n");
			}
			sleep_ms(500);
		}
		printf("1111111111111111111111111111\n");
		sleep_ms(10000);
		printf("before disconnect:%lld\n", getSysTick());
		ret = disconnect(COMMON_INTERFACE_SOCKET0);
		printf("after disconnect:%lld ret:%d\n", getSysTick(), ret);
		while(1);
	}
	*/
	/*
	{
		check_wlan(115200, "CMCC-Netcore", "6028silion");
		while(1);
	}
	*/
	/*
	{
		uint32 *number = (uint32 *)(0x20026FF0+4);
		printf("entern main\n");
		
		if ((*number) > 1000)
			(*number) = 0;
		printf("number:%u\n", *number);
		(*number)++;
		sleep_ms(2000);
		printf("number:%u\n", *number);
		sleep_ms(1000);
		system_reset();
	}
	*/
/*
	{
		int rc = 0;
		Network n;
		MQTTClient c;
		MQTTMessage m;
		uint64 ck_timer;
		uint64 lastReconn;
		
		char pubbuf[100];
		uint8_t buf[100];
		gIsTlsConn = 1;
		gMbedNetFd = COMMON_INTERFACE_UART1;
		
		NewNetwork(&n, COMMON_INTERFACE_UART1, gIsTlsConn);
		init_wlan(NULL, 0, 1);
//		init_4g("222.128.15.242", 8883, 0);
Reconn:
		while (1) 
		{
			DisconnectNetwork(&n);
			if (ConnectNetwork(&n, targetIP, targetPort) == 0)
				break;
			else
				sleep_ms(2000);
		}
		
		MQTTClientInit(&c, &n, 1000, buf, sizeof(buf), tempBuffer, sizeof(tempBuffer));

		MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
		data.willFlag = 0;
		data.MQTTVersion = 3;
		data.clientID.cstring = opts.clientid;
		data.username.cstring = opts.username;
		data.password.cstring = opts.password;

		data.keepAliveInterval = 60;
		data.cleansession = 1;

		rc = MQTTConnect(&c, &data);
		printf("Connected %d\r\n", rc);
		opts.showtopics = 1;
		if (rc != 0)
		{
			goto Reconn;
		}
		printf("Subscribing to %s\r\n", MQTT_TOPIC);
		rc = MQTTSubscribe(&c, MQTT_TOPIC, opts.qos, messageArrived);
		printf("Subscribed %d\r\n", rc);
		if (rc != 0)
		{
			goto Reconn;
		}
		
		m.qos = QOS2;
		m.retained = 0;
		m.dup = 0;
		ck_timer = getSysTick();
		lastReconn = ck_timer;
		
		while(1)
		{
			rc = MQTTYield(&c, 3000);
			printf("MQTTYield rc:%d\n", rc);
			if (rc == -1)
				goto Reconn;

			if(ck_timer + 10000 < getSysTick())
			{
				ck_timer = getSysTick();

				printf("Publishing to %s\r\n", MQTT_TOPIC);

				sprintf(pubbuf, "Hello, W6100! @ck_timer(%lld)", ck_timer);
				m.payload = pubbuf;
				m.payloadlen = strlen(pubbuf);
				rc = MQTTPublish(&c, MQTT_TOPIC, &m);
				printf("Published %d\r\n", rc);
				if (rc != 0)
					goto Reconn;
			}

		}
	}
	*/
	/*
	{
		int contsuccnt = 0;
		char *json = "{\"test\":[{\"name\":\"liuhan\",\"age\":10,\"skill\":[\"C++\",\"C#\",\"Java\",\"Delphi\"]},{\"name\":\"liuhan\",\"age\":10,\"skill\":[\"C++\",\"C#\",\"Java\",\"Delphi\"]},{\"name\":\"liuhan\",\"age\":10,\"skill\":[\"C++\",\"C#\",\"Java\",\"Delphi\"]},{\"name\":\"liuhan\",\"age\":10,\"skill\":[\"C++\",\"C#\",\"Java\",\"Delphi\"]},{\"name\":\"liuhan\",\"age\":10,\"skill\":[\"C++\",\"C#\",\"Java\",\"Delphi\"]}]}";
		init_mem_sta();
		init_wlan(NULL, 0, 1);//devapi.qweather.com //222.128.15.242
//		init_4g("222.128.15.242", 10025, 0);
		init_http_fn();
		gSerIp[0] = 192;
		gSerIp[1] = 168;
		gSerIp[2] = 1;
		gSerIp[3] = 44;
		gSerPort = 443;
		gIsTlsConn = 1;
		gMbedNetFd = COMMON_INTERFACE_UART1;
		SockRecvBuffer = malloc_hexp(CmdRecvBufLen);
		if (gIsTlsConn == 1)
			init_mbedtls();
		
		if (gMbedNetFd == COMMON_INTERFACE_SOCKET0)
			CheckServerConnection();

		while(1)
		{
			if (http_upload_test(COMMON_INTERFACE_UART1, (uint8 *)json, strlen(json)) != 0)
				contsuccnt = 0;
			else{
				contsuccnt++;
				printf("--------------- contsuccnt:%d\n", contsuccnt);				
			}
			sleep_ms(3000);
		}
	}
	*/
	/*
	{
		uint8 serip[] = {222,128,15,242};
		uint16 serport = 10100;
		init_4g(serip, serport);
		while(1);
	}*/
	if (gCurWorkMode == WorkMode_ActVer_1 || 
		gCurWorkMode == WorkMode_ActVer_2)
	{
		gRtSetting = malloc_hexp(sizeof(ReaderRunTimeSettings_ST));
		setdef_rdr_runtime_settings(gRtSetting);
		
		if (gCurWorkMode == WorkMode_ActVer_1)
		{
			AppCustomParams *pAppCusParam = malloc_hexp(sizeof(AppCustomParams));
			if (get_active_mode_config(pAppCusParam) == 0)
			{
				AppCustomParams_To_static_settings(pAppCusParam, gPRdrStaSet);
				AppCustomParams_To_runtime_settings(pAppCusParam, gRtSetting);
				ispassive = 0;
			}
			else
				TRACE("get_active_mode_config error\n");
			free_hexp(pAppCusParam);
		}
		else if (gCurWorkMode == WorkMode_ActVer_2)
		{
			if (get_rdr_runtime_settings(gRtSetting) != 0)
			{
				TRACE("get_rdr_runtime_settings error\n");
				free_hexp(gRtSetting);
			}
			else
			{
				if (gRtSetting->upload.hw_inf != 0)
					ispassive = 0;
			}
		}
	}
		
	for (i = 0; i < 4; ++i)
	{
		if (gPRdrStaSet->gpos[i] != 0)
			gpo_set(i+1, gPRdrStaSet->gpos[i]-1);
		else
			gpo_set(i+1, DefNoSndGPOVal);
	}
		
	if (ispassive == 1)
	{
		TRACE("run user_main_passive\n");			
		gBrdCstDevInfo.workmode = 1;
		user_main_passive();
	}
	else
	{
		TRACE("run user_main_active\n");
		gBrdCstDevInfo.workmode = 2;
		user_main_active();
	}

//	user_main_active();
}
