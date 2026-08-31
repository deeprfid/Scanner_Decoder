#ifndef Web_Utility_H
#define Web_Utility_H

#ifdef __cplusplus
extern "C" {
#endif


#include "ModuleReader.h"
typedef enum
{
	HMApiErr_Ok = 0,

	HMApiErr_Rdr_IOErr = 20002,
	HMApiErr_Param_Err = 20003,
	HMApiErr_Param_Missing = 20004,
	HMApiErr_Param_NotSupported = 20005,

	HMApiErr_Reader_Busy = 20010,
	HMApiErr_Invalid_Op = 20011,
	HMApiErr_Invalid_SockIndex = 20012,
	HMApiErr_Init_Rdr = 20013,

	HMApiErr_Conf_FileMissing = 20020,
	HMApiErr_Conf_ParamErr = 20021,
	HMApiErr_Conf_WriteFailed = 20022,
	HMApiErr_Conf_DnsError = 20023,
	HMApiErr_Conf_FormatError = 20024,
	HMApiErr_Conf_ReadFailed = 20025,
	HMApiErr_Conf_NoActiveMode = 20026,

	HMApiErr_Curl_gInitFailed = 20050,
	HMApiErr_Curl_eInitFailed = 20051,

	HMApiErr_OSApi_CreatTheadFailed = 20100,
	HMApiErr_OSApi_Socket = 20101,
	HMApiErr_OSApi_ioctl = 20102,
	HMApiErr_OSApi_fork = 20103,
	HMApiErr_OSApi_setsid= 20104,
	HMApiErr_OSApi_setsockopt = 20105,
	HMApiErr_OSApi_bind = 20106,
	HMApiErr_OSApi_listen = 20107,
	HMApiErr_OSApi_fopen = 20108,
	HMApiErr_OSApi_fwrite = 20109,
	HMApiErr_OSApi_chdir = 20110,
	HMApiErr_OSApi_shmget = 20111,
	HMApiErr_OSApi_shmat = 20112,
	HMApiErr_OSApi_shmdt = 20113,
	HMApiErr_OSApi_remove = 20114,

	HMApiErr_UpdFw_ExeInsScriptFailed = 20200,
	HMApiErr_UpdFw_ExeScriptCmdFailed = 20201,
	HMApiErr_UpdFw_InvalidFwFileFormat = 20202,
	HMApiErr_UpdFw_RecvFwFileError = 20203,
	HMApiErr_UpdFw_StopSlsysFailed = 20204,
	HMApiErr_UpdFw_UpdateModFailed = 20205,
	HMApiErr_UpdFw_UpdateModProcFailed = 20206,
	HMApiErr_UpdFw_CreateUpMProcFailed = 20207,
	HMApiErr_UpdFw_WriteVerFileFailed = 20208,

	HMApiErr_Mqtt_Conf_FileMissing = 20300,
	HMApiErr_Mqtt_InitFailed = 20301,
	HMApiErr_Max_val = 0x7fffffff,
} HttpMApiErrCode;

void GetHardwareString(HardwareDetails &hd, char *board, char *mod);

#define httpAPIErrCodeBase 100000

#define CommonMonet_SendfailedThresh 4
#define CustomMonet_SendfailedThresh 20

void Reboot();

HttpMApiErrCode MTECode2HMAEcode(READER_ERR err);

const char *HMECode2String(HttpMApiErrCode err, int hreader);
HttpMApiErrCode GetRdrIdentifier(char *RdrIdfier, int fd);
extern char* hrlErrInfo;

READER_ERR OpenReader();
extern int gAntNumber;
extern int ghReader;
typedef struct
{
	unsigned char main_board;
	unsigned char rfid_mod;
	unsigned char software_version[4];
	unsigned char antcount;
	unsigned char connected_antennas[16];
	int hb_count;
} HeartBeatData_ST;
extern HeartBeatData_ST gHbData;

#ifdef __cplusplus
}
#endif

#endif

