#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Utility.h"
#include "APIHttpRequest.h"
#include "hc32f46_driver.h"

HttpMApiErrCode GetRdrIdentifier(char *RdrIdfier, int fd)
{
	if (fd <= COMMON_INTERFACE_SOCKET2)
		sprintf(RdrIdfier, "reader/%d.%d.%d.%d", gNetConf.ip[0], 
			gNetConf.ip[1], gNetConf.ip[2], gNetConf.ip[3]);
	else if (fd == COMMON_INTERFACE_UART1)
		sprintf(RdrIdfier, "reader/%d.%d.%d.%d", gWlanNet.ip[0], 
			gWlanNet.ip[1], gWlanNet.ip[2], gWlanNet.ip[3]);
	else
		sprintf(RdrIdfier, "reader");
	return HMApiErr_Ok;
}

void Str2Hex(const char *buf, int len, unsigned char *hexbuf)
{
	memset(hexbuf , 0, len/2);
	for (int i = 0; i < len; ++i)
	{
		unsigned char hex = 0;
		if ((buf[i] >= '0') && (buf[i] <= '9'))
			hex = buf[i] - '0';
		else if ((buf[i] >= 'A') && (buf[i] <= 'F'))
			hex = buf[i] - 'A' + 10;
		else if ((buf[i] >= 'a') && (buf[i] <= 'f'))
			hex = buf[i] - 'a' + 10;
		hexbuf[i/2] |= (hex & 0xf) << (((i+1)%2)*4);
	}
}

void GetHardwareString(HardwareDetails &hd, char *board, char *mod)
{
	if (hd.board == MAINBOARD_ARM9_V2)
		strcpy(board, "arm9_v2");
	else if (hd.board == MAINBOARD_ARM9_WIFI_V2)
		strcpy(board, "arm9_wifi_v2");
	else if (hd.board == MAINBOARD_ARM9)
		strcpy(board, "arm9");
	else if (hd.board == MAINBOARD_ARM9_WIFI)
		strcpy(board, "arm9_wifi");
	else
		strcpy(board, "hc32f46");


	if (hd.module == MODOULE_M6E)
		strcpy(mod, "m6e");
	else if (hd.module == MODOULE_M6E_PRC)
		strcpy(mod, "m6e_prc");
	else if (hd.module == MODOULE_M6E_MICRO)
		strcpy(mod, "m6e_micro");
	else if (hd.module == MODOULE_SLR1100)
		strcpy(mod, "slr1100");
	else if (hd.module == MODOULE_SLR1200)
		strcpy(mod, "slr1200");
	else if (hd.module == MODOULE_SLR5100)
		strcpy(mod, "slr5100");
	else if (hd.module == MODOULE_SLR5300)
		strcpy(mod, "slr5300");
	else if (hd.module == MODOULE_SLR5200)
		strcpy(mod, "slr5200");

	else if (hd.module == MODOULE_SIM3100)
		strcpy(mod, "sim3100");
	else if (hd.module == MODOULE_SIM5100)
		strcpy(mod, "sim5100");
	else if (hd.module == MODOULE_SIM7100)
		strcpy(mod, "sim7100");

	else if (hd.module == MODOULE_SIM3200)
		strcpy(mod, "sim3200");
	else if (hd.module == MODOULE_SIM5200)
		strcpy(mod, "sim5200");
	else if (hd.module == MODOULE_SIM7200)
		strcpy(mod, "sim7200");

	else if (hd.module == MODOULE_SLR5800)
		strcpy(mod, "slr5800");
	else if (hd.module == MODOULE_SLR5900)
		strcpy(mod, "slr5900");
	else if (hd.module == MODOULE_SLR6000)
		strcpy(mod, "slr6000");
	else if (hd.module == MODOULE_SLR6100)
		strcpy(mod, "slr6100");	
	else
		strcpy(mod, "unknown");
}


void Reboot()
{
	system_reset();
}

HttpMApiErrCode MTECode2HMAEcode(READER_ERR err)
{
	return (HttpMApiErrCode)(err + httpAPIErrCodeBase);
}

char* hrlErrInfo;
const char *HMECode2String(HttpMApiErrCode err, int hreader)
{
	if (err == httpAPIErrCodeBase+MT_FREQUENT_ERR)
		return LookupErrorString(err - httpAPIErrCodeBase);

	if (err >= httpAPIErrCodeBase)
	{
		if (hreader > 0)
		{
			char *mapierrstr;
			int code2conv;
			if (GetLastDetailError(hreader, &code2conv, &mapierrstr) != MT_OK_ERR)
			{
				return "MT_INVALID_READER_HANDLE";
			}
			else
			{
				if (code2conv == 0x0505)
				{
					unsigned char tmpbuf[255];
					ParamGet(hreader, MTR_PARAM_READER_ERRORDATA, tmpbuf);
					sprintf(hrlErrInfo, "%s-ant_%d", mapierrstr, tmpbuf[1]);
					return hrlErrInfo;
				}
				else
					return mapierrstr;
			}
		}
		else
			return LookupErrorString(err - httpAPIErrCodeBase);

	}
	else
	{
		switch (err)
		{
		case HMApiErr_Ok:
			return "ok";
		case HMApiErr_Rdr_IOErr:
			return "Reader IO error";
		case HMApiErr_Invalid_Op:
				return "Invalid opertaion";
		case HMApiErr_Invalid_SockIndex:
				return "Invalid SockIndex Internal err";
		case HMApiErr_Init_Rdr:
				return "Init reader failed";
		case	HMApiErr_Reader_Busy:
				return "Reader busy, asynchronous read in progress";
		case HMApiErr_Conf_FileMissing:
				return "Missing configuration file";
		case HMApiErr_Conf_ParamErr:
				return "Configuration file parameter error";
		case HMApiErr_Conf_WriteFailed:
				return "Write configuration file failed";
		case HMApiErr_Conf_DnsError:
				return "Dns configuration error";
		case HMApiErr_Conf_FormatError:
				return "Configuration format error";
		case HMApiErr_Conf_ReadFailed:
				return "Read configuration file failed";
		case HMApiErr_Conf_NoActiveMode:
				return "Missing active mode configuration file";
		case HMApiErr_Curl_gInitFailed:
				return "Init curl globle object failed";
		case HMApiErr_Curl_eInitFailed:
				return "Init curl easy object failed";
		case HMApiErr_OSApi_CreatTheadFailed:
				return "OS api call CreatThead failed";
		case HMApiErr_OSApi_Socket:
				return "OS api call Socket failed";
		case HMApiErr_OSApi_ioctl:
				return "OS api call ioctl failed";
		case HMApiErr_OSApi_fork:
				return "OS api call fork failed";
		case HMApiErr_OSApi_setsid:
				return "OS api call setsid failed";
		case HMApiErr_OSApi_setsockopt:
				return "OS api call setsockopt failed";
		case HMApiErr_OSApi_bind:
				return "OS api call bind failed";
		case HMApiErr_OSApi_listen:
				return "OS api call listen failed";
		case HMApiErr_OSApi_fopen:
				return "OS api call fopen failed";
		case HMApiErr_OSApi_fwrite:
				return "OS api call fwrite failed";
		case HMApiErr_OSApi_chdir:
				return "OS api call chdir failed";
		case HMApiErr_OSApi_shmget:
				return "OS api call shmget failed";
		case HMApiErr_OSApi_shmat:
				return "OS api call shmat failed";
		case HMApiErr_OSApi_shmdt:
				return "OS api call shmdt failed";
		case HMApiErr_OSApi_remove:
				return "OS api call remove failed";
		case HMApiErr_UpdFw_ExeInsScriptFailed:
				return "Execute insall script failed";
		case HMApiErr_UpdFw_ExeScriptCmdFailed:
				return "Execute script command failed";
		case HMApiErr_UpdFw_InvalidFwFileFormat:
				return "Firmware file format error";
		case HMApiErr_UpdFw_RecvFwFileError:
				return "Recvice firmware file error";
		case HMApiErr_UpdFw_StopSlsysFailed:
				return "Stop rifd engine failed";
		case HMApiErr_UpdFw_UpdateModFailed:
				return "Update rfid module firmware failed";
		case HMApiErr_UpdFw_UpdateModProcFailed:
				return "Update rfid module process failed";
		case HMApiErr_UpdFw_CreateUpMProcFailed:
				return "Create update rfid module process failed";
		case HMApiErr_UpdFw_WriteVerFileFailed:
				return "Write version file failed";
		case HMApiErr_Mqtt_Conf_FileMissing:
			return "Mqtt configuration files lost";
		case HMApiErr_Mqtt_InitFailed:
			return "Mqtt init failed";
		default:
			return (char*)"Unkown error";
		}
	}
}







