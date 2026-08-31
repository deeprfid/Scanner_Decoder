#ifndef HttpModuleAPI_H
#define HttpModuleAPI_H
#include "ModuleReader.h"
#include "hc32f46_driver.h"
#include "json-parser.h"
#include "Utility.h"
HttpMApiErrCode MTECode2HMAEcode(READER_ERR err);

class HttpModuleAPI
{
public:
	HttpModuleAPI();
	HttpMApiErrCode Init();
	int create_buffer(int itcnt, int msize);
	int httpAPIDispatch(char *url, char *json, int len);
	void httpParamSet(char *json, int len, bool issavefile = true);
	int httpGetChunkedHeader(int fd);
	int httpGetNextChunked();
	

	int getOpError()
	{
		return (int)m_OpError;
	}
	char *GetRespBuf()
	{
		return m_RespJsonBuffer;
	}

	char *getReaderIdentifier()
	{
		return m_RdrAddr;
	}
	
	int getRdrPhyantportnumber()
	{
		return m_phyantportnumber;
	}

	int getReaderHandle()
	{
		return m_hReader;
	}
	void ResetTagBuffer();

	static void AsyncRead_Thread(void * arg);
	
private:
	void httpParamGet(char *json, int len);
	void httpSyncInventory(char *json, int len);
	void httpStartAsyncInventory(char *json, int len);
	void httpStopAsyncInventory(char *json, int len);
	void httpGetAsynctags(char *json, int len);
	void httpReadTagBank(char *json, int len);
	void httpWriteTagBank(char *json, int len);
	void httpWriteTagEpc(char *json, int len);
	void httpLockTag(char *json, int len);
	void httpKillTag(char *json, int len);
	void httpGetGPI(char *json, int len);
	void httpSetGPO(char *json, int len);
	void httpReboot(char *json, int len);
	void httpResetRfidModule(char *json, int len);
  void httpSaveTagMethod(char *json, int len);
	void httpEascfg(char *json, int len);
	void preInventory(json_value *jvalue, int *antennas, int &antcount);
	void preTagOperation(json_value *jvalue, int *antenna, 
	unsigned char *acspwd, unsigned char **ppwd, int *timeout);
	void setTagFilter(json_value *jvalue, bool isset = true);
	void setEmbData(json_value *jvalue, bool isset = true);
	int validinvants(int *ants, int antcnt);
	char *ParamErr2String(HttpMApiErrCode err);

#define MaxRespJsonBufLen (1024*4)
#define OtherAddJsonBufPos 120
#define MaxErrPnameBufLen 100
#define MaxTagOpBankBufLen 256
#define MaxPhyAntPortCount 16

	int m_phyantportnumber;
	char m_RdrAddr[128];
	int m_TagCount;
	int m_hReader;
	HttpMApiErrCode m_OpError;
	bool m_IsConnectReader;
	bool m_IsCreateBuffer;
	unsigned short m_MaxPwr;
	unsigned short m_MinPwr;
	bool m_IsCheckError;
	char m_RespJsonBuffer[MaxRespJsonBufLen];
	char *m_AddOtherJsonBuf;
	char m_ReqType[30];
//	hapi_TagInfoBuffer m_TagBuffer;
	unsigned char m_TagBankData[256];
	char m_StrConvertBuffer[600];
	bool m_IsAddOtherJson;
	bool m_IsInvTags;
	bool m_IsFirstTag;
	bool m_IsFinChunked;
  bool m_IsUploadTags;
	char m_ErrParamName[MaxErrPnameBufLen];

	HardwareDetails m_HWdetails;

	osSemaphoreId_t m_AsyncReadSem;
	osRtxSemaphore_t m_AsyncReadSem_cb;
	osThreadId_t m_ThIdAsRd;
	volatile READER_ERR m_AsyncReadStartErr;
	volatile READER_ERR m_AsyncReadStopErr;
	volatile READER_ERR m_AsyncReadGetErr;
	osMutexId_t m_TagbufMux;
	osRtxMutex_t m_TagbufMux_cb;
	int m_AsyncInvAnts[MaxPhyAntPortCount];
	int  m_AsyncInvAntsCnt;
	
	volatile bool m_IsAsyncRead;
	bool m_IsAsyncOp;
	
	int m_tbIsUniByAnt;
	int m_tbIsUniByEmddata;
	int m_tbIsRecHighestRssi;
	
	int m_tbMaxRecLength;
};

#endif

