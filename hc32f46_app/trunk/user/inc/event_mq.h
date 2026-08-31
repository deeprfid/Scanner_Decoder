#ifndef _EVENT_MQ_H
#define _EVENT_MQ_H
#include "ModuleReader.h"
#include "hc32f46_driver.h"

typedef enum
{
	App_Evt_None = 0,
	App_Evt_Heartbeat = 1,
	App_Evt_BatchMoment = 2,
	App_Evt_GpiChange = 3,
	App_Evt_TagComing = 4,
	App_Evt_RdrError = 5,
} App_Evt_Code;

void init_evt_sys(void);
int put_evt_que(TAGINFO *evt);
int get_evt_que(TAGINFO *evt);
void event_generator(void *arg);

extern uint8 gIsEvtTagRead;
extern uint8 gIsEvtTagComing;
extern uint8 gIsEvtHeartbeat;
extern uint8 gIsSendTagOnly;
extern uint8 gIsEvtEmptyData;
extern uint8 gIsEvtSyncTimeReq;
extern uint8 gIsEvtGpiChan;
extern uint8 gIsSendRdrErr;
extern uint64 gHbLastTime;
extern uint64 gBatchLastTime;

#endif


