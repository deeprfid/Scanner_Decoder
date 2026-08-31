#include "hc32f46_driver.h"
#include "common.h"
#include <string.h>
#include <stdlib.h>

#define SilionMACBase1 0x08
#define SilionMACBase2 0x26
#define SilionMACBase3 0xAE
#define SilionMACBase4 0x10


const uint8 DefIpAddr[] = {192, 168, 1, 100};
const uint8 DefSubnetMask[] = {255, 255, 255, 0};
const uint8 DefGateWay[] = {192, 168, 1, 254};
const uint8 DefDnsServer[] = {223, 6, 6, 6};
const uint8 DefMac[6] = {0x1E, 0x30, 0x6C, 0xA2, 0x45, 0x5E};
const uint16 DefTcpPort = 8080;
//#define NetConfig_Len 24
#define NetConfig_Marker "NetConfig"

#define MaxNetConfigLen 100
#define MaxWorkConfigLen 1536
#define MaxPassiveModeConfigLen 1024
#define MaxBootConfigLen 312

typedef struct
{
	uint8 *netcfgbuf;
	uint8 *workcfgbuf;
	uint8 *passivemodecfgbuf;
	uint8 *bootcfgbuf;
} ConfReadBuffer;

int pre_set_config(ConfReadBuffer *pbuffer)
{
	pbuffer->netcfgbuf = malloc(MaxNetConfigLen);
	pbuffer->workcfgbuf = malloc(MaxWorkConfigLen);
	pbuffer->passivemodecfgbuf = malloc(MaxPassiveModeConfigLen);
	pbuffer->bootcfgbuf = malloc(MaxBootConfigLen);
	
	if (pbuffer->netcfgbuf == NULL || pbuffer->workcfgbuf== NULL || 
		pbuffer->bootcfgbuf == NULL || pbuffer->passivemodecfgbuf == NULL)
	{
		printf("set_network_config:no enough memory\n");
		return -1;
	}
	
	flash_bytes_read(NetConfig_Addr, pbuffer->netcfgbuf, MaxNetConfigLen);
	flash_bytes_read(WorkConfig_Addr, pbuffer->workcfgbuf, MaxWorkConfigLen);
	flash_bytes_read(PassiveModeConfig_Addr, pbuffer->passivemodecfgbuf, MaxPassiveModeConfigLen);
	flash_bytes_read(BootConfig_Addr, pbuffer->bootcfgbuf, MaxBootConfigLen);
	
	return 0;
}
int set_config_to_flash(ConfReadBuffer *pbuffer)
{
	int err = 0;
	
	E(flash_bytes_write(NetConfig_Addr, pbuffer->netcfgbuf, MaxNetConfigLen));
	E(flash_bytes_write(WorkConfig_Addr, pbuffer->workcfgbuf, MaxWorkConfigLen));
	E(flash_bytes_write(PassiveModeConfig_Addr, pbuffer->passivemodecfgbuf, MaxPassiveModeConfigLen));
	E(flash_bytes_write(BootConfig_Addr, pbuffer->bootcfgbuf, MaxBootConfigLen));
	
	free(pbuffer->netcfgbuf);
	free(pbuffer->workcfgbuf);
	free(pbuffer->passivemodecfgbuf);
	free(pbuffer->bootcfgbuf);

FIN:
	return err;
}

int set_network_config(networkParaConfig *para)
{
	int pos;
	int err = 0;
	ConfReadBuffer confBuf;
	
	E(pre_set_config(&confBuf));

	if (para->listenPort == 0)
		para->listenPort = DefTcpPort;
	if (para->dnsServer[0] == 0x00 && para->dnsServer[1] == 0x00 && 
		para->dnsServer[2] == 0x00 && para->dnsServer[3] == 0x00)
		memcpy(para->dnsServer, DefDnsServer, 4);
	
	strcpy((char *)confBuf.netcfgbuf, NetConfig_Marker);
	pos = strlen(NetConfig_Marker);
	memcpy(confBuf.netcfgbuf+pos, para->ip, 4);
	pos += 4;
	memcpy(confBuf.netcfgbuf+pos, para->subnetMask, 4);
	pos += 4;
	memcpy(confBuf.netcfgbuf+pos,  para->gatewayIP, 4);	
	pos += 4;
	
	if (!(confBuf.netcfgbuf[pos] == SilionMACBase1 && 
		confBuf.netcfgbuf[pos+1] == SilionMACBase2 && 
			confBuf.netcfgbuf[pos+2] == SilionMACBase3 && 
		(confBuf.netcfgbuf[pos+3] & 0xF0) == SilionMACBase4))
		memcpy(confBuf.netcfgbuf+pos, para->mac, 6);
	pos += 6;
	
	SetNumU16((uint8*)confBuf.netcfgbuf+pos, para->listenPort);
	pos += 2;
	memcpy(confBuf.netcfgbuf+pos, para->dnsServer, 4);
	pos += 4;
	
	E(set_config_to_flash(&confBuf));
	
FIN:
	return err;
}

int get_network_config(networkParaConfig *para)
{
	int pos;
	uint8 *netcfgbuf = malloc(MaxNetConfigLen);
	
	if (netcfgbuf == NULL)
	{
		printf("get_network_config:no enough memory\n");
		return -1;
	}
	
	flash_bytes_read(NetConfig_Addr, netcfgbuf, MaxNetConfigLen);
	
	if (strncmp((char *)netcfgbuf, NetConfig_Marker, strlen(NetConfig_Marker)) != 0)
	{
		memcpy(para->ip, DefIpAddr, 4);
		memcpy(para->subnetMask, DefSubnetMask, 4);
		memcpy(para->gatewayIP, DefGateWay, 4);
		memcpy(para->mac, DefMac, 6);
		para->listenPort = DefTcpPort;
		memcpy(para->dnsServer, DefDnsServer, 4);
	}
	else
	{
		pos = strlen(NetConfig_Marker);
		memcpy(para->ip, netcfgbuf+pos, 4);
		pos += 4;
		memcpy(para->subnetMask, netcfgbuf+pos, 4);
		pos += 4;
		memcpy(para->gatewayIP, netcfgbuf+pos, 4);
		pos += 4;
		memcpy(para->mac, netcfgbuf+pos, 6);
		pos += 6;
		para->listenPort = GetNumU16(netcfgbuf+pos);
		pos += 2;
		memcpy(para->dnsServer, netcfgbuf+pos, 4);
	}
	
	free(netcfgbuf);
	return 0;
}

typedef enum
{
	FwUpdateMode_Default = 0,
	FwUpdateMode_ByFtp = 1,
} FwUpdateModeCode;

typedef struct
{
	uint32 updateflag;
	uint32 firmwareaddr;
	uint32 firmwaresize;
	uint32 firmwarecrc;
	uint32 firmwarever;
	FwUpdateModeCode updatemode;
	uint16 ftpport;
	uint8 ftpaddr[256];
	
	uint32 btParamscrc;
} BtParams_ST;


int getBtParams(BtParams_ST *params)
{	
	int pos = 0;
	uint8 *bootcfgbuf = malloc(MaxBootConfigLen);
	
	if (bootcfgbuf == NULL)
	{
		printf("getBtParams:no enough memory\n");
		return -1;
	}
	
	flash_bytes_read(BootConfig_Addr, bootcfgbuf, MaxBootConfigLen);
	

	params->updateflag = GetNumU32(bootcfgbuf+pos);
	pos += 4;
	params->firmwareaddr = GetNumU32(bootcfgbuf+pos);
	pos += 4;
	params->firmwaresize = GetNumU32(bootcfgbuf+pos);
	pos += 4;
	params->firmwarecrc = GetNumU32(bootcfgbuf+pos);
	pos += 4;
	params->firmwarever = GetNumU32(bootcfgbuf+pos);
	pos += 4;
	params->updatemode = GetNumU32(bootcfgbuf+pos);
	pos += 4;
	params->ftpport = GetNumU16(bootcfgbuf+pos);
	pos += 2;
	memcpy(params->ftpaddr, bootcfgbuf+pos, 256);
	pos += 256;
	
	params->btParamscrc = GetNumU32(bootcfgbuf+pos);
	pos += 4;

	free(bootcfgbuf);
	
	if (params->btParamscrc == params->updateflag + params->firmwareaddr
		+ params->firmwaresize + params->firmwarecrc + params->firmwarever +123)
		return 0;
	else
		return -1;
}

void firmware_version(uint8 *version)
{

	BtParams_ST btparam;
	int ver = 0;
	if(version)
	{
		if(getBtParams(&btparam) == 0)
			ver = btparam.firmwarever;
		version[0] = (ver >> 24) & 0xff;
		version[1] = (ver >> 16) & 0xff;
		version[2] = (ver >> 8) & 0xff;
		version[3] = (ver >> 0) & 0xff;
	}
}

