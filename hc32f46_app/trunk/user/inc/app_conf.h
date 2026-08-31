#ifndef _APP_CONF_H_
#define _APP_CONF_H_

#include "hc32f46_driver.h"
#include "ModuleReader.h"


#define httpAPIErrCodeBase 100000

extern int CmdRecvBufLen;
extern uint8 *SockRecvBuffer;
extern uint8 *SockSendBuffer;
extern ReaderRunTimeSettings_ST *gRtSetting;
extern ReaderStaticSettings_ST *gPRdrStaSet;
extern volatile int gRdrStateFlag;
extern volatile int gErrSend;

extern uint8 gSerIp[4];
extern uint16 gSerPort;
extern uint64 glastpulsetm;
extern READER_ERR gRdrErr;
extern ConnAnts_ST gConnants;
extern int ghReader;
extern int gCanAsyncInv;
extern WorkMode_Code gCurWorkMode;
extern int gIsStartAsyncInv;

extern int TagSendBufLen;
extern void (*g_wg_send_fn)(uint8, uint8, uint8 *);
extern uint8 gWgGytes;
//#define MaxPasModeTbMemSize (1024*74)
//#define MaxPasModeTbMemSize (1024*54)
//#define MaxActModeTbMemSize (1024*94)
//#define MaxActModeTbMemSize (1024*60)
#define JsonParseMemSize (1024*20)
#define AppDubugPrintf 0
#define DynMemReserveSize (1024*30)
#define MbedTLSDynMemSize (1024*35)
#define ReaderCppMemSize (1024*3)
//#define MaxMsgQueObjCnt 60
#define MqttRecvBufLen 1880
#define MaxUpsendFailedCntBefReset 500


//蔡盼定制
#define Custom_By_Caipan 0
//中行信息，招商银行项目
#define Custom_By_ZHXX_ZSYH 0
//长达智能物联
#define Custom_By_CDZNWL 0
//广州拓迪 
#define Custom_By_GZTD 0
//杭州维芯智能
#define Custom_By_HZWXZN 0
//shenzhen BMA 
#define Custom_By_SZBMA 1
#endif



