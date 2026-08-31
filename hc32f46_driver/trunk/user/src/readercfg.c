#include <string.h>
#include <stdlib.h>
#include "hc32f46_driver.h"
#include "driverconfig.h"
#include "readercfg.h"
#include "json-parser.h"
#include "common.h"

void set_eastag_to_flash(void); 
const uint8 DefIpAddr[] = {192, 168, 0, 120};
const uint8 DefSubnetMask[] = {255, 255, 255, 0};
const uint8 DefGateWay[] = {192, 168, 0, 1};
const uint8 DefDnsServer[] = {223, 6, 6, 6};
const uint8 DefMac[6] = {0x1E, 0x30, 0x6C, 0xA2, 0x45, 0x5E};
//const uint8 DefMac[6] = {0x08, 0x26, 0xAE, 0x10, 0x00, 0x00};
const uint16 DefTcpPort = 8080;
//#define NetConfig_Len 24

int pre_set_config(ConfReadBuffer *pbuffer)
{
	pbuffer->netcfgbuf = malloc_hexp(MaxNetConfigLen);
	pbuffer->wificfgbuf = malloc_hexp(MaxWifiConfigLen);
	pbuffer->hwtypecfgbuf = malloc_hexp(MaxHwTypeConfigLen);
	pbuffer->monetcfgbuf = malloc_hexp(MaxMonetConfigLen);
	pbuffer->bluetoothcfgbuf = malloc_hexp(MaxBluetoothConfigLen);
	pbuffer->activemodecfgbuf = malloc_hexp(MaxActiveModeConfigLen);
	pbuffer->workmodeparambuf = malloc_hexp(MaxWorkModeParamLen);
	pbuffer->passivemodecfgbuf = malloc_hexp(MaxPassiveModeConfigLen);
	pbuffer->bootcfgbuf = malloc_hexp(MaxBootConfigLen);
	
	if (pbuffer->netcfgbuf == NULL || pbuffer->activemodecfgbuf== NULL || 
		pbuffer->bootcfgbuf == NULL || pbuffer->passivemodecfgbuf == NULL || 
		pbuffer->workmodeparambuf == NULL || pbuffer->bluetoothcfgbuf == NULL || 
		pbuffer->monetcfgbuf == NULL)
	{
		TRACE("set_network_config:no enough memory\n");
		return -1;
	}
	
	flash_bytes_read(NetConfig_Addr, pbuffer->netcfgbuf, MaxNetConfigLen);
	flash_bytes_read(WifiConfig_Addr, pbuffer->wificfgbuf, MaxWifiConfigLen);
	flash_bytes_read(HwTypeConfig_Addr, pbuffer->hwtypecfgbuf, MaxHwTypeConfigLen);
	flash_bytes_read(MonetConfig_Addr, pbuffer->monetcfgbuf, MaxMonetConfigLen);
	flash_bytes_read(BluetoothConfig_Addr, pbuffer->bluetoothcfgbuf, MaxBluetoothConfigLen);
	flash_bytes_read(ActiveModeConfig_Addr, pbuffer->activemodecfgbuf, MaxActiveModeConfigLen);
	flash_bytes_read(WorkModeParams_Addr, pbuffer->workmodeparambuf, MaxWorkModeParamLen);
	flash_bytes_read(PassiveModeConfig_Addr, pbuffer->passivemodecfgbuf, MaxPassiveModeConfigLen);
	flash_bytes_read(BootConfig_Addr, pbuffer->bootcfgbuf, MaxBootConfigLen);
	
	return 0;
}

int set_config_to_flash(ConfReadBuffer *pbuffer)
{
	int err = 0;
	E(flash_sector_erase(RdrConfigPage_Addr));
	E(flash_bytes_write(NetConfig_Addr, pbuffer->netcfgbuf, MaxNetConfigLen));
	E(flash_bytes_write(WifiConfig_Addr, pbuffer->wificfgbuf, MaxWifiConfigLen));
	E(flash_bytes_write(HwTypeConfig_Addr, pbuffer->hwtypecfgbuf, MaxHwTypeConfigLen));
	E(flash_bytes_write(MonetConfig_Addr, pbuffer->monetcfgbuf, MaxMonetConfigLen));
	E(flash_bytes_write(BluetoothConfig_Addr, pbuffer->bluetoothcfgbuf, MaxBluetoothConfigLen));
	E(flash_bytes_write(ActiveModeConfig_Addr, pbuffer->activemodecfgbuf, MaxActiveModeConfigLen));
	E(flash_bytes_write(WorkModeParams_Addr, pbuffer->workmodeparambuf, MaxWorkModeParamLen));
	E(flash_bytes_write(PassiveModeConfig_Addr, pbuffer->passivemodecfgbuf, MaxPassiveModeConfigLen));
	E(flash_bytes_write(BootConfig_Addr, pbuffer->bootcfgbuf, MaxBootConfigLen));
	
FIN:
	free_hexp(pbuffer->netcfgbuf);
	free_hexp(pbuffer->wificfgbuf);
	free_hexp(pbuffer->hwtypecfgbuf);
	free_hexp(pbuffer->monetcfgbuf);
	free_hexp(pbuffer->bluetoothcfgbuf);
	free_hexp(pbuffer->activemodecfgbuf);
	free_hexp(pbuffer->workmodeparambuf);
	free_hexp(pbuffer->passivemodecfgbuf);
	free_hexp(pbuffer->bootcfgbuf);
	return err;
}

int set_multi_config(uint8 *netBytes, int netLen, 
	uint8 *pasveBytes, int pasveLen, uint8 *actBytes, int actLen)
{
	int err = 0;
	ConfReadBuffer confBuf;
	
	E(pre_set_config(&confBuf));
	if (netBytes != NULL)
		memcpy(confBuf.netcfgbuf, netBytes, netLen);
	if (pasveBytes != NULL)
		memcpy(confBuf.passivemodecfgbuf, pasveBytes, pasveLen);
	if (actBytes != NULL)
	{
		/*
		int i;
		printf("set_multi_config wdata:");
		for (i = 0; i < 20; ++i)
			TRACE("%02X ", actBytes[i]);
		printf("\n");
		*/
		memcpy(confBuf.activemodecfgbuf, actBytes, actLen);
	}
	
	E(set_config_to_flash(&confBuf));
FIN:
	return err;	
}

void set_default_network_config(void)
{
	networkParaConfig networkPara;
	memset(&networkPara, 0, sizeof(networkPara));
	get_network_config(&networkPara);
	memcpy(networkPara.ip, DefIpAddr, 4);
	memcpy(networkPara.subnetMask, DefSubnetMask, 4);
	memcpy(networkPara.gatewayIP, DefGateWay, 4);
	if (!(networkPara.mac[0] == SilionMACBase1 && 
		networkPara.mac[1] == SilionMACBase2 && 
			networkPara.mac[2] == SilionMACBase3 && 
		(networkPara.mac[3] & 0xF0) == SilionMACBase4))
		memcpy(networkPara.mac, DefMac, 6);

	networkPara.listenPort = DefTcpPort;
	memcpy(networkPara.dnsServer, DefDnsServer, 4);
	set_network_config(&networkPara);
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

void get_passivemode_config(int offset, uint8 *pData, int len)
{
	flash_bytes_read(PassiveModeConfig_Addr+offset, pData, len);
}

int set_passivemode_config(int offset, uint8 *pData, int len)
{
	int err = 0;
	ConfReadBuffer confBuf;
	
	E(pre_set_config(&confBuf));
	memcpy(confBuf.passivemodecfgbuf+offset, pData, len);
	E(set_config_to_flash(&confBuf));
FIN:
	return err;	
}

int get_network_config(networkParaConfig *para)
{
//	int i;
	int pos;
	uint8 *netcfgbuf = malloc_hexp(MaxNetConfigLen);
	
	if (netcfgbuf == NULL)
	{
		TRACE("get_network_config:no enough memory\n");
		return -1;
	}
	
	flash_bytes_read(NetConfig_Addr, netcfgbuf, MaxNetConfigLen);
	/*
	printf("netcfgbuf:");
	for (i = 0; i < MaxNetConfigLen; ++i)
		printf("%d ", netcfgbuf[i]);
	printf("\n");
	*/
	if (strncmp((char *)netcfgbuf, NetConfig_Marker, strlen(NetConfig_Marker)) != 0)
	{
		TRACE("use default network setting\n");
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
	
	free_hexp(netcfgbuf);
	return 0;
}
void dump_network_config(networkParaConfig *para)
{
#ifdef _DEBUG
	TRACE("dump_network_config start\n");
	TRACE("ip:%d.%d.%d.%d\n", para->ip[0], para->ip[1], para->ip[2], para->ip[3]);
	TRACE("netmake:%d.%d.%d.%d\n", para->subnetMask[0], 
		para->subnetMask[1], para->subnetMask[2], para->subnetMask[3]);
	TRACE("gateway:%d.%d.%d.%d\n", para->gatewayIP[0], 
		para->gatewayIP[1], para->gatewayIP[2], para->gatewayIP[3]);
	TRACE("dns:%d.%d.%d.%d\n", para->dnsServer[0], 
		para->dnsServer[1], para->dnsServer[2], para->dnsServer[3]);
	TRACE("mac: %02X:%02X:%02X:%02X:%02X:%02X\n", para->mac[0], 
		para->mac[1], para->mac[2], para->mac[3], para->mac[4], para->mac[5]);
	TRACE("dump_network_config end\n");
#endif
}

int is_netconf_dhcp(networkParaConfig *para)
{
	 if (get_network_config(para) != 0)
	 {
		 TRACE("is_netconf_dhcp get_network_config error\n");
		 return -1;
	 }
	 if (para->ip[0] == 0 && para->ip[1] == 0 && 
		para->ip[2] == 0 && para->ip[3] == 0)
	 {
//		 memcpy(para->ip, DefIpAddr, 4);
//		 memcpy(para->subnetMask, DefSubnetMask, 4);
//		 memcpy(para->gatewayIP, DefGateWay, 4);
		return 1;
	 }
	 else
		return 0;
}


int getBtParams(BtParams_ST *params)
{	
	int pos = 0;
	int ftpuserlen;
	int ftppwdlen;
	int ftpseraddrlen;
	int fnamelen;
	int ret = 0;
	int paratotlen;
	int i;
	uint32 btcrc = 0;
	uint32 btcrcOnflh = 0;
	
	uint8 *bootcfgbuf = malloc_hexp(MaxBootConfigLen);
	
	if (bootcfgbuf == NULL)
	{
		TRACE("getBtParams:no enough memory\n");
		return -1;
	}
	flash_bytes_read(BootConfig_Addr, bootcfgbuf, MaxBootConfigLen);
	
	pos = strlen(BtParams_Marker);
	if (memcmp(bootcfgbuf, BtParams_Marker, pos) != 0)
	{
		TRACE("getBtParams: BtParams_Marker error\n");
		ret = -1;
		goto FIN;
	}
	
	paratotlen = GetNumU16(bootcfgbuf+pos);
	pos += 2;
	
	if (paratotlen + pos + 4 > MaxBootConfigLen)
	{
		TRACE("getBtParams: paratotlen error\n");
		ret = -1;
		goto FIN;
	}
	
	for (i = 0; i < paratotlen+strlen(BtParams_Marker)+2; ++i)
		btcrc += bootcfgbuf[i];
	
	btcrcOnflh = GetNumU32(bootcfgbuf+pos+paratotlen);
//	printf("0000 crc pos:%d, paratotlen:%d, btcrc:%08X, btcrcOnflh:%08X\n", 
//		pos+paratotlen, paratotlen, btcrc, btcrcOnflh);
	if (btcrcOnflh != btcrc)
	{
		TRACE("getBtParams: btcrc error\n");
		ret = -1;
		goto FIN;		
	}
	
	params->updateflag = bootcfgbuf[pos++];
	params->firmwareaddr = GetNumU32(bootcfgbuf+pos);
	pos += 4;
	params->firmwaresize = GetNumU32(bootcfgbuf+pos);
	pos += 4;
	params->firmwarecrc = GetNumU32(bootcfgbuf+pos);
	pos += 4;
	params->firmwarever = GetNumU32(bootcfgbuf+pos);
	pos += 4;
	params->updatemode = (FwUpdateModeCode)bootcfgbuf[pos++];
	
	ftpuserlen = bootcfgbuf[pos++];
	memcpy(params->ftpuser, bootcfgbuf+pos, ftpuserlen);
	params->ftpuser[ftpuserlen] = 0;
	pos += ftpuserlen;
	
	ftppwdlen = bootcfgbuf[pos++];
	memcpy(params->ftppassword, bootcfgbuf+pos, ftppwdlen);
	params->ftppassword[ftppwdlen] = 0;
	pos += ftppwdlen;
	
	ftpseraddrlen = bootcfgbuf[pos++];
	memcpy(params->ftpaddr, bootcfgbuf+pos, ftpseraddrlen);
	params->ftpaddr[ftpseraddrlen] = 0;
	pos += ftpseraddrlen;
	
	fnamelen = bootcfgbuf[pos++];
	memcpy(params->filename, bootcfgbuf+pos, fnamelen);
	params->filename[fnamelen] = 0;
	pos += fnamelen;
	
	params->btParamscrc = GetNumU32(bootcfgbuf+pos);
	pos += 4;
	
FIN:
	free_hexp(bootcfgbuf);
	return ret;
}

int setBtParams(BtParams_ST *params)
{
	int pos;
	int err = 0;
	int ftpuserlen;
	int ftppwdlen;
	int ftpseraddrlen;
	int fnamelen;
	ConfReadBuffer confBuf;
	int i;
	uint32 btcrc = 0;
	
	E(pre_set_config(&confBuf));

	
	strcpy((char *)confBuf.bootcfgbuf, BtParams_Marker);
	pos = strlen(BtParams_Marker)+2;
	
	confBuf.bootcfgbuf[pos++] = params->updateflag;
	SetNumU32(confBuf.bootcfgbuf+pos, params->firmwareaddr);
	pos += 4;
	SetNumU32(confBuf.bootcfgbuf+pos, params->firmwaresize);
	pos += 4;
	SetNumU32(confBuf.bootcfgbuf+pos, params->firmwarecrc);
	pos += 4;
	SetNumU32(confBuf.bootcfgbuf+pos, params->firmwarever);
	pos += 4;	
	confBuf.bootcfgbuf[pos++] = params->updatemode;

	ftpuserlen = strlen(params->ftpuser);
	confBuf.bootcfgbuf[pos++] = ftpuserlen;
	memcpy(confBuf.bootcfgbuf+pos, params->ftpuser, ftpuserlen);
	pos += ftpuserlen;
	
	ftppwdlen = strlen(params->ftppassword);
	confBuf.bootcfgbuf[pos++] = ftppwdlen;
	memcpy(confBuf.bootcfgbuf+pos, params->ftppassword, ftppwdlen);
	pos += ftppwdlen;
	
	ftpseraddrlen = strlen(params->ftpaddr);
	confBuf.bootcfgbuf[pos++] = ftpseraddrlen;	
	memcpy(confBuf.bootcfgbuf+pos, params->ftpaddr, ftpseraddrlen);
	pos += ftpseraddrlen;
	
	fnamelen = strlen(params->filename);
	confBuf.bootcfgbuf[pos++] = fnamelen;	
	memcpy(confBuf.bootcfgbuf+pos, params->filename, fnamelen);
	pos += fnamelen;
	
	SetNumU16(confBuf.bootcfgbuf+strlen(BtParams_Marker), 
		pos-strlen(BtParams_Marker)-2);
		
	for (i = 0; i < pos; ++i)
		btcrc += confBuf.bootcfgbuf[i];
	
//	printf("1111 crc pos:%d, paramlen:%d, btcrc:%08X\n", pos, pos-strlen(BtParams_Marker)-2, btcrc);
	SetNumU32(confBuf.bootcfgbuf+pos, btcrc);
//	pos += 4;
		
	E(set_config_to_flash(&confBuf));
	E(getBtParams(params));
FIN:
	return err;	
}

void firmware_upgrade(uint8 flag)
{
	BtParams_ST btparams;
	getBtParams(&btparams);
	btparams.updateflag = flag;
	setBtParams(&btparams);
	sleep_ms(100);
	system_reset();
}

void erase_active_mode_params(void)
{
	erase_multi_config(ERASE_FLS_CFG_BIT_ACTMODE);
}

void dumpBtParams(BtParams_ST *params)
{
	TRACE("BtParams dump start\n");
	TRACE("updateflag : %d\n", params->updateflag);
	TRACE("firmwareaddr : %d\n", params->firmwareaddr);
	TRACE("firmwaresize : %d\n", params->firmwaresize);
	TRACE("firmwarecrc : %08X\n", params->firmwarecrc);
	TRACE("firmwarever : %d\n", params->firmwarever);
	
	TRACE("updatemode : %d\n", params->updatemode);
	TRACE("ftpuser : %s\n", params->ftpuser);
	TRACE("ftppassword : %s\n", params->ftppassword);
	TRACE("ftpaddr : %s\n", params->ftpaddr);
	TRACE("filename : %s\n", params->filename);
	
	TRACE("btParamscrc : %d\n", params->btParamscrc);
	TRACE("BtParams dump end\n");
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

int erase_multi_config(uint16 erasebits)
{
	int err = 0;
	ConfReadBuffer confBuf;
	uint8 zerobytes[30] = {0x00};
	
	if (erasebits == 0)
		return 0;
	
	E(pre_set_config(&confBuf));
	if ((erasebits & ERASE_FLS_CFG_BIT_ETHERNET) == ERASE_FLS_CFG_BIT_ETHERNET)
	{
		int pos = 0;
		memcpy(confBuf.netcfgbuf+pos, NetConfig_Marker, strlen(NetConfig_Marker));
		pos += strlen(NetConfig_Marker);
		memcpy(confBuf.netcfgbuf+pos, DefIpAddr, 4);
		pos += 4;
		memcpy(confBuf.netcfgbuf+pos, DefSubnetMask, 4);
		pos += 4;
		memcpy(confBuf.netcfgbuf+pos, DefGateWay, 4);
		pos += 4;
		memcpy(confBuf.netcfgbuf+pos, gNetConf.mac, 6);			
		pos += 6;
		SetNumU16((uint8*)confBuf.netcfgbuf+pos, DefTcpPort);
		pos += 2;
		memcpy(confBuf.netcfgbuf+pos, DefDnsServer, 4);
	}
	
	if ((erasebits & ERASE_FLS_CFG_BIT_WLAN) == ERASE_FLS_CFG_BIT_WLAN)
		memcpy(confBuf.wificfgbuf, zerobytes, sizeof(zerobytes));
	
	if ((erasebits & ERASE_FLS_CFG_BIT_HWTYPE) == ERASE_FLS_CFG_BIT_HWTYPE)
		memcpy(confBuf.hwtypecfgbuf, zerobytes, sizeof(zerobytes));

	if ((erasebits & ERASE_FLS_CFG_BIT_BLUETOOTH) == ERASE_FLS_CFG_BIT_BLUETOOTH)
		memcpy(confBuf.bluetoothcfgbuf, zerobytes, sizeof(zerobytes));
	
	if ((erasebits & ERASE_FLS_CFG_BIT_ACTMODE) == ERASE_FLS_CFG_BIT_ACTMODE)
		memcpy(confBuf.activemodecfgbuf, zerobytes, sizeof(zerobytes));
		
	if ((erasebits & ERASE_FLS_CFG_BIT_WKMODEPARA) == ERASE_FLS_CFG_BIT_WKMODEPARA)
		memcpy(confBuf.workmodeparambuf, zerobytes, sizeof(zerobytes));
	
	if ((erasebits & ERASE_FLS_CFG_BIT_PSVMODE) == ERASE_FLS_CFG_BIT_PSVMODE)
	{
		TRACE("if ((erasebits & ERASE_FLS_CFG_BIT_PSVMODE) == ERASE_FLS_CFG_BIT_PSVMODE)\n");
		memcpy(confBuf.passivemodecfgbuf, zerobytes, sizeof(zerobytes));
	}
	
	E(set_config_to_flash(&confBuf));
	
FIN:
	return err;
}

int erase_wlan_config(void)
{
	int err = 0;
	ConfReadBuffer confBuf;
	E(pre_set_config(&confBuf));
	memset(confBuf.wificfgbuf, 0, 32);
	E(set_config_to_flash(&confBuf));
FIN:
	return err;
	
}

BoardComponents_ST gBoardCompos;
Spi_Ex_Code get_spi_ex_dev(void)
{
	return (Spi_Ex_Code)gBoardCompos.spi_ex;
}

int get_board_compos(void)
{
	int pos = strlen(HwTypeConfig_Marker);
	uint8 *buf = malloc_hexp(MaxHwTypeConfigLen);
	
	if (buf == NULL)
	{
		TRACE("get_board_compos:no enough memory\n");
		return -1;
	}
	
	flash_bytes_read(HwTypeConfig_Addr, buf, MaxHwTypeConfigLen);
	if (memcmp(HwTypeConfig_Marker, buf, pos) == 0)
	{
		gBoardCompos.spi_ex = buf[pos++];
		gBoardCompos.uart_ex = buf[pos++];
		free_hexp(buf);
	}
	else
	{
		gBoardCompos.spi_ex = Spi_Ex_None;
		gBoardCompos.uart_ex = Uart_Ex_None;
		free_hexp(buf);
		return -1;
	}
	/*
	gBoardCompos.spi_ex = Spi_Ex_Ethernet;
//	gBoardCompos.uart1_ex = Uart1_Ex_Bluetooth;
	gBoardCompos.uart1_ex = Uart1_Ex_None;
//	gBoardCompos.uart1_ex = Uart1_Ex_Wlan;
	*/
	return 0;
}

Uart_Ex_Code get_uart_ex_dev(void)
{
	return (Uart_Ex_Code)gBoardCompos.uart_ex;
}

void dump_wlan_config(WlanConfig_ST *wlanst)
{
#ifdef _DEBUG
	TRACE("dump_wlan_config start\n");
	dump_network_config(&wlanst->ipinfo);
	TRACE("ssid:%s\n", wlanst->ssid);
	TRACE("mode:%d\n", wlanst->mode);
//	printf("encry:%d\n", wlanst->encry);
	TRACE("pwd:%s\n", wlanst->pwd);
	TRACE("dump_wlan_config end\n");
#endif
}

int get_wlan_config(WlanConfig_ST *wlanst)
{
	int pos = strlen(WifiConfig_Marker);
	char *buf = malloc_hexp(MaxWifiConfigLen);
	
	if (buf == NULL)
	{
		TRACE("get_wlan_config:no enough memory\n");
		return -1;
	}
	
	setdef_wlan_config(wlanst);
	
	flash_bytes_read(WifiConfig_Addr, buf, MaxWifiConfigLen);
	if (memcmp(WifiConfig_Marker, buf, pos) == 0)
	{
		int jsonlen = GetNumU16((uint8 *)buf+pos);
		int vlret;
		pos += 2;
		vlret = valid_wlan_config(buf+pos, jsonlen, wlanst);
		memcpy(wlanst->ipinfo.mac, gWlanNet.mac, 6);
		TRACE("get_wlan_config valid_wlan_config vlret:%d\n", vlret);
		free_hexp(buf);
		if (vlret != 0)
			setdef_wlan_config(wlanst);
		return vlret;
	}
	else
	{
		setdef_wlan_config(wlanst);
		memcpy(wlanst->ipinfo.mac, gWlanNet.mac, 6);
		TRACE("not find WifiConfig_Marker\n");
		free_hexp(buf);
		return -1;
	}
}

const uint8 DefAPAddr[] = {10, 0, 0, 1};
const uint8 DefAPSubnetMask[] = {255, 255, 255, 0};
const uint8 DefAPGateWay[] = {0, 0, 0, 0};
const uint8 DefAPDns[] = {0, 0, 0, 0};
#define DefAPSSID "UHF-Reader"

void setdef_wlan_config(WlanConfig_ST *wlanst)
{
	memcpy(wlanst->ipinfo.ip, DefAPAddr, 4);
	memcpy(wlanst->ipinfo.subnetMask, DefAPSubnetMask, 4);
	memcpy(wlanst->ipinfo.gatewayIP, DefAPGateWay, 4);
	memcpy(wlanst->ipinfo.dnsServer, DefAPDns, 4);
	wlanst->ipinfo.listenPort = DefTcpPort;
	wlanst->mode = Wlan_WMode_AP;
	strcpy(wlanst->ssid, DefAPSSID);
	wlanst->pwd[0] = 0;
}

#if IS_RTOS2_SUPPORT
#define PARSE_CHK_IN_TAB(elm, name, item, tab, exist) \
	do { \
		if (json_getint(elm, name, &pvalint) == 0) \
		{ \
			if (is_in_inttab(tab, sizeof(tab)/4, pvalint) == 0) \
			{ \
				TRACE("%s is invalid\n", #name); \
				return -1;	\
			} \
			else \
				item = pvalint; \
		} \
		else \
		{ \
			if (exist == 1) \
			{ \
				TRACE("%s dose not find\n", #name); \
				return -1; \
			} \
		} \
	} while (0)
	

#define PARSE_CHK_RANGE(obj, name, item, min, max, exist) \
	do { \
		exflag = 0; \
		if (json_getint(obj, name, &pvalint) == 0) \
		{ \
			if (pvalint < min || pvalint > max) \
			{ \
				TRACE("%s is invalid\n", #name); \
				return -1;	\
			} \
			else \
			{ \
				exflag = 1; \
				item = pvalint; \
			} \
		} \
		else \
		{ \
			if (exist == 1) \
			{ \
				TRACE("%s dose not find\n", #name); \
				return -1; \
			} \
		} \
	} while (0)
	
#define PARSE_CHK_STR(obj, name, item, len, exist, canempty) \
	do { \
		exflag = 0; \
		if (json_getstring_len(obj, name, len, 2, item) != 0) \
		{ \
			if (exist == 1) \
			{ \
				TRACE("%s dose not find\n", #name); \
				return -1; \
			} \
		} \
		else \
		{ \
			if (item[0] == 0 && canempty == 0) \
			{ \
				TRACE("%s len is invalid\n", #name); \
				return -1; \
			} \
			exflag = 1; \
		} \
	} while (0)

int is_in_inttab(int *tab, int len, int val)
{
	int i;
	for (i = 0; i < len; ++i)
	{
		if (val == tab[i])
			return 1;
	}
	return 0;
}

int valid_ipinfo_cfg(json_value *obj, networkParaConfig *ipinfo)
{
	char strbuf[30];
	int pvalint;
	int exflag;
	
	PARSE_CHK_STR(obj, "ip", strbuf, 29, 1, 0);
	if (addr_str2bin(strbuf, ipinfo->ip) != 0)
	{
		TRACE("ip format error\n");
		return -1;
	}

	PARSE_CHK_STR(obj, "nm", strbuf, 29, 1, 0);
	if (addr_str2bin(strbuf, ipinfo->subnetMask) != 0)
	{
		TRACE("nm format error\n");
		return -1;
	}

	PARSE_CHK_STR(obj, "gw", strbuf, 29, 1, 0);
	if (addr_str2bin(strbuf, ipinfo->gatewayIP) != 0)
	{
		TRACE("gw format error\n");
		return -1;
	}

	PARSE_CHK_STR(obj, "dns", strbuf, 29, 0, 0);
	if (exflag)
	{
		if (addr_str2bin(strbuf, ipinfo->dnsServer) != 0)
		{
			TRACE("dns format error\n");
			return -1;
		}
	}
		
	PARSE_CHK_STR(obj, "mac", strbuf, 29, 0, 0);
	if (exflag)
	{
		if (strlen(strbuf) != 12)
		{
			TRACE("mac len must be 12\n");
			return -1;				
		}
		uint8 machex[6];
		if (strTohex(strbuf, 12, machex) != 0)
		{
			TRACE("mac format error\n");
			return -1;	
		}
			
		if (!(machex[0] == 0xff && machex[1] == 0xff && 
			machex[2] == 0xff && machex[3] == 0xff && 
			machex[4] == 0xff && machex[5] == 0xff ))
			memcpy(ipinfo->mac, machex, 6);
	}
	
	PARSE_CHK_RANGE(obj, "lport", ipinfo->listenPort, 100, 65530, 0);
	return 0;
}

int valid_wlan_config_byobj(json_value *jvalue, WlanConfig_ST *wlanst)
{
	int pvalint;
	int exflag;
	
	if (valid_ipinfo_cfg(jvalue, &wlanst->ipinfo) != 0)
	{
		TRACE("ipinfo is invalid\n");
		return -1;		
	}
	
	PARSE_CHK_RANGE(jvalue, "mode", wlanst->mode, 1, 2, 1);
	if (wlanst->mode == Wlan_WMode_AP)
	{
		if (wlanst->ipinfo.ip[0] == 0 && wlanst->ipinfo.ip[1] == 0 && 
			wlanst->ipinfo.ip[2] == 0 && wlanst->ipinfo.ip[3] == 0)
		{
			TRACE("dhcp dose not allow with ap mode\n");
			return -1;			
		}
	}
	
	PARSE_CHK_STR(jvalue, "ssid", wlanst->ssid, 32, 1, 0);
	/*
	PARSE_CHK_RANGE(jvalue, "auth", wlanst->auth, 0, 3, 1);
	
	PARSE_CHK_RANGE(jvalue, "encry", wlanst->encry, 0, 4, 1);
	if (wlanst->encry == Wlan_Encry_NONE)
	{
		if (wlanst->auth != Wlan_Auth_OPEN)
		{
			TRACE("auth should be Wlan_Auth_OPEN\n");
			return -1;
		}
	}
	else if (wlanst->encry == Wlan_Encry_TKIP || 
		wlanst->encry == Wlan_Encry_AES)
	{
		if (!(wlanst->auth == Wlan_Auth_WPAPSK || 
			wlanst->auth == Wlan_Auth_WPA2PSK))
		{
			TRACE("auth should be Wlan_Auth_WPAPSK or Wlan_Auth_WPA2PSK\n");
			return -1;
		}
	}
	else if (wlanst->encry == Wlan_Encry_WEP_A || 
		wlanst->encry == Wlan_Encry_WEP_H)
	{
		if (wlanst->auth != Wlan_Auth_SHARED)
		{
			TRACE("auth should be Wlan_Auth_SHARED\n");
			return -1;
		}
	}
	*/
	PARSE_CHK_STR(jvalue, "pwd", wlanst->pwd, 64, 0, 1);
	if (!exflag)
		wlanst->pwd[0] = 0;
	/*
	keylen = strlen(wlanst->key);
	if (keylen == 0)
	{
		if (wlanst->auth != Wlan_Auth_OPEN)
		{
			TRACE("key is none, auth is not Wlan_Auth_OPEN\n");
			return -1;
		}
	}
	else
	{
		if (wlanst->auth == Wlan_Auth_WPAPSK || 
				wlanst->auth == Wlan_Auth_WPA2PSK)
		{
			if (keylen < 8)
			{
				TRACE("key len is invalid, less than 8\n");
				return -1;	
			}
		}
		else
		{
			if (wlanst->encry == Wlan_Encry_WEP_A)
			{
				if (!(keylen == 5 || keylen == 13))
				{
					TRACE("key len is invalid, should be 5 or 13\n");
					return -1;	
				}
			}
			else if (wlanst->encry == Wlan_Encry_WEP_H)
			{
				if (keylen == 10 || keylen == 26)
				{
					int i;
					for (i = 0; i < keylen; ++i)
					{
						if (!((wlanst->key[i] >= '0' &&  wlanst->key[i] <= '9') || 
							(wlanst->key[i] >= 'A' &&  wlanst->key[i] <= 'F')))
						{
							TRACE("key should be hex string\n");
							return -1;							
						}
					}
				}
				else
				{
					TRACE("key len is invalid, should be 10 or 26\n");
					return -1;	
				}
			}
		}
	}*/
	return 0;
}
#endif

int get_json_field(char *buf, char *field, int type, char *strbuf)
{
	char *tmp1,*tmp2;
	char field_[50];
	
	sprintf(field_, "\"%s\":", field);
	tmp1 = strstr(buf, field_);
	if (tmp1 == NULL)
		return -1;

	if (type == 1)
	{
		tmp2 = strstr(tmp1+strlen(field_)+1, "\"");
		if (tmp1 == NULL)
			return -1;

		memcpy(strbuf, tmp1+strlen(field_)+1, tmp2-(tmp1+strlen(field_)+1));
		strbuf[tmp2-(tmp1+strlen(field_)+1)] = 0;
	}
	else if (type == 2)
	{
		tmp2 = strstr(tmp1+strlen(field_), ",");
		if (tmp1 == NULL)
			return -1;

		memcpy(strbuf, tmp1+strlen(field_), tmp2-(tmp1+strlen(field_)));
		strbuf[tmp2-(tmp1+strlen(field_))] = 0;
	}
	else
		return -1;

	return 0;
}

int valid_wlan_config(char *buf, int blen, WlanConfig_ST *wlanst)
{
#if IS_RTOS2_SUPPORT
	json_value *jvalue;

	jvalue = json_parse(buf, blen);
	if (jvalue == NULL)
	{
		TRACE("valid_wlan_config jvalue == NULL\n");
		return -1;
	}
	return valid_wlan_config_byobj(jvalue, wlanst);
#else
	char strbuf[100];
	
	buf[blen] = 0;
//	TRACE("valid_wlan_config:%s\n", buf);
	if (get_json_field(buf, "ip", 1, strbuf) == 0)
	{
		if (addr_str2bin(strbuf, wlanst->ipinfo.ip) != 0)
		{
			TRACE("ip format error\n");
			return -1;
		}
	}
	else
		return -1;

	if (get_json_field(buf, "nm", 1, strbuf) == 0)
	{
		if (addr_str2bin(strbuf, wlanst->ipinfo.subnetMask) != 0)
		{
			TRACE("nm format error\n");
			return -1;
		}	
	}
	else
		return -1;
	
	if (get_json_field(buf, "gw", 1, strbuf) == 0)
	{
		if (addr_str2bin(strbuf, wlanst->ipinfo.gatewayIP) != 0)
		{
			TRACE("gw format error\n");
			return -1;
		}	
	}
	else
		return -1;
	
	if (get_json_field(buf, "dns", 1, strbuf) == 0)
	{
		if (addr_str2bin(strbuf, wlanst->ipinfo.dnsServer) != 0)
		{
			TRACE("dns format error\n");
			return -1;
		}	
	}
	else
		return -1;

	if (get_json_field(buf, "lport", 2, strbuf) == 0)
		wlanst->ipinfo.listenPort = atoi_Arm(strbuf);
	else
		return -1;
	
	if (get_json_field(buf, "mode", 2, strbuf) == 0)
		wlanst->mode = atoi_Arm(strbuf);
	else
		return -1;
	if (wlanst->mode == Wlan_WMode_AP)
	{
		if (wlanst->ipinfo.ip[0] == 0 && wlanst->ipinfo.ip[1] == 0 && 
			wlanst->ipinfo.ip[2] == 0 && wlanst->ipinfo.ip[3] == 0)
		{
			TRACE("dhcp dose not allow with ap mode\n");
			return -1;			
		}
	}

	if (get_json_field(buf, "ssid", 1, wlanst->ssid) != 0)
		return -1;
	
	if (get_json_field(buf, "pwd", 1, wlanst->pwd) != 0)
		wlanst->pwd[0] = 0;
	
	return 0;
#endif
}

extern uint8 gBleMac[6];
int get_bluetooth_config(BluetoothConfig_ST *blest)
{
	int pos = strlen(BluetoothConfig_Marker);
	char *buf = malloc_hexp(MaxBluetoothConfigLen);
	
	if (buf == NULL)
	{
		TRACE("get_bluetooth_config:no enough memory\n");
		return -1;
	}
	
	memcpy(blest->mac, gBleMac, 6);
	setdef_bluetooth_config(blest);
	flash_bytes_read(BluetoothConfig_Addr, buf, MaxBluetoothConfigLen);
	if (memcmp(BluetoothConfig_Marker, buf, pos) == 0)
	{
		int jsonlen = GetNumU16((uint8 *)buf+pos);
		int vlret;
		pos += 2;
		vlret = valid_bluetooth_config(buf+pos, jsonlen, blest);
		memcpy(blest->mac, gBleMac, 6);
		TRACE("get_bluetooth_config valid_bluetooth_config vlret:%d\n", vlret);
		free_hexp(buf);
		if (vlret != 0)
			setdef_bluetooth_config(blest);
		return vlret;
	}
	else
	{
		setdef_bluetooth_config(blest);
		memcpy(blest->mac, gBleMac, 6);
		TRACE("not find BluetoothConfig_Marker\n");
		free_hexp(buf);
		return -1;
	}
}

void setdef_bluetooth_config(BluetoothConfig_ST *blest)
{
	strcpy(blest->name, "UHF-Reader");
	blest->std_ble_pair = 0;
	blest->pwd_pair = 0;
}

void dump_monet_config(MonetConfig_ST *monst)
{
#ifdef _DEBUG
	TRACE("dump_monet_config start\n");
	TRACE("apn_name:%s\n", monst->apn.name);
	TRACE("apn_user:%s\n", monst->apn.user);
	TRACE("apn_pwd:%s\n", monst->apn.pwd);
	
	TRACE("apn_auth:%d\n", monst->apn.auth);
	TRACE("apn_cid:%d\n", monst->apn.cid);
	TRACE("apn_enable:%d\n", monst->apn.enable);
	TRACE("dump_monet_config end\n");	
#endif	
}

int erase_bluetooth_config(void)
{
	int err = 0;
	ConfReadBuffer confBuf;
	E(pre_set_config(&confBuf));
	memset(confBuf.bluetoothcfgbuf, 0, 32);
	E(set_config_to_flash(&confBuf));
FIN:
	return err;
}

#if IS_RTOS2_SUPPORT
int valid_monet_config_byobj(json_value *jvalue, 
	MonetConfig_ST *monst)
{
	int pvalint;
	int exflag;
	json_value *apnobj;
	
	if (json_getobject(jvalue, "apn", &apnobj) == 0)
	{
		PARSE_CHK_STR(apnobj, "name", monst->apn.name, (MONET_APN_NAME_LEN-1), 1, 0);
		
		PARSE_CHK_STR(apnobj, "user", monst->apn.user, (MONET_APN_USER_LEN-1), 0, 1);
		
		PARSE_CHK_STR(apnobj, "pwd", monst->apn.pwd, (MONET_APN_PWD_LEN-1), 0, 1);
		
		if (exflag == 1)
			monst->apn.pwd[MONET_APN_PWD_LEN-1] = 0;
		
		PARSE_CHK_RANGE(apnobj, "auth", monst->apn.auth, 0, 2, 0);
		
		PARSE_CHK_RANGE(apnobj, "cid", monst->apn.cid, 0, 6, 0);	
		
		PARSE_CHK_RANGE(apnobj, "enable", monst->apn.enable, 0, 1, 0);	
	}

	return 0;
}

int valid_bluetooth_config_byobj(json_value *jvalue, 
	BluetoothConfig_ST *blest)
{
	int pvalint;
	int exflag;
	
	PARSE_CHK_STR(jvalue, "name", blest->name, 20, 0, 0);
	
	PARSE_CHK_RANGE(jvalue, "std_ble_pair", blest->std_ble_pair, 0, 1, 0);
	
	PARSE_CHK_RANGE(jvalue, "pwd_pair", blest->pwd_pair, 0, 1, 0);
	
	if (blest->pwd_pair == 1)
	{
		PARSE_CHK_STR(jvalue, "pwd", blest->pwd, 6, 1, 0);
		if (exflag == 1)
			blest->pwd[6] = 0;
	}
	
	return 0;
}
#endif

void setdef_monet_config(MonetConfig_ST *monst)
{
	strcpy(monst->apn.name, "CMNET");
	monst->apn.user[0] = 0;
	monst->apn.pwd[0] = 0;
	monst->apn.auth = 0;
	monst->apn.cid = 1;
	monst->apn.enable = 0;
}

int get_monet_config(MonetConfig_ST *monst)
{
	int pos = strlen(MonetConfig_Marker);
	char *buf = malloc_hexp(MaxMonetConfigLen);
	
	if (buf == NULL)
	{
		TRACE("get_monet_config:no enough memory\n");
		return -1;
	}
	
	setdef_monet_config(monst);
	flash_bytes_read(MonetConfig_Addr, buf, MaxMonetConfigLen);
	if (memcmp(MonetConfig_Marker, buf, pos) == 0)
	{
		int jsonlen = GetNumU16((uint8 *)buf+pos);
		int vlret;
		pos += 2;
		vlret = valid_monet_config(buf+pos, jsonlen, monst);
		TRACE("get_monet_config valid_monet_config vlret:%d\n", vlret);
		free_hexp(buf);
		if (vlret != 0)
			setdef_monet_config(monst);
		return vlret;
	}
	else
	{
		setdef_monet_config(monst);
		TRACE("not find MonetConfig_Marker\n");
		free_hexp(buf);
		return -1;
	}
}

int valid_monet_config(char *buf, int blen, MonetConfig_ST *monst)
{
#if IS_RTOS2_SUPPORT
	json_value *jvalue;

	jvalue = json_parse(buf, blen);
	if (jvalue == NULL)
	{
		TRACE("valid_monet_config jvalue == NULL\n");
		return -1;
	}
	return valid_monet_config_byobj(jvalue, monst);
#else
	char strbuf[100];
	
	if (get_json_field(buf, "name", 1, monst->apn.name) == 0)
		return -1;
	
	if (get_json_field(buf, "user", 1, monst->apn.user) == 0)
		return -1;
	
	if (get_json_field(buf, "pwd", 1, monst->apn.user) == 0)
		return -1;
		
	if (get_json_field(buf, "auth", 2, strbuf) == 0)
		monst->apn.auth = atoi_Arm(strbuf);
	else
		return -1;

	if (get_json_field(buf, "cid", 2, strbuf) == 0)
		monst->apn.cid = atoi_Arm(strbuf);
	else
		return -1;
	
	if (get_json_field(buf, "enable", 2, strbuf) == 0)
		monst->apn.enable = atoi_Arm(strbuf);
	else
		return -1;
	
	return 0;
#endif
}

int valid_bluetooth_config(char *buf, int blen, BluetoothConfig_ST *blest)
{
#if IS_RTOS2_SUPPORT
	json_value *jvalue;

	jvalue = json_parse(buf, blen);
	if (jvalue == NULL)
	{
		TRACE("valid_bluetooth_config jvalue == NULL\n");
		return -1;
	}
	return valid_bluetooth_config_byobj(jvalue, blest);
#else
	char strbuf[100];
	
	if (get_json_field(buf, "name", 1, blest->name) == 0)
		return -1;
	
	if (get_json_field(buf, "std_ble_pair", 2, strbuf) == 0)
		blest->std_ble_pair = atoi_Arm(strbuf);
	else
		return -1;

	if (get_json_field(buf, "pwd_pair", 2, strbuf) == 0)
		blest->pwd_pair = atoi_Arm(strbuf);
	else
		return -1;
	
	if (blest->pwd_pair == 1)
	{
		if (get_json_field(buf, "pwd", 1, blest->name) != 0)
			blest->pwd[6] = 0;
	}
	
	return 0;
#endif
}

#if IS_RTOS2_SUPPORT
const char *ACPMagicStr = "StdModeActParam";

int get_active_mode_config(AppCustomParams *pACP)
{
	unsigned char *pConf;
	int pos = 0;
	int i;
	uint8 *actcfgbuf = malloc_hexp(MaxActiveModeConfigLen);
	
	if (actcfgbuf == NULL)
	{
		TRACE("get_active_mode_config:no enough memory\n");
		return -1;
	}
	flash_bytes_read(ActiveModeConfig_Addr, actcfgbuf, MaxActiveModeConfigLen);
	
	memset(pACP, 0, sizeof(AppCustomParams));
	
	if (memcmp(actcfgbuf, ACPMagicStr, 15) != 0)
	{
		free_hexp(actcfgbuf);
		return -1;
	}
	
	pConf = actcfgbuf + 17;
	

	if (!(pConf[pos] == 0xFF && pConf[pos+1] == 0x00))
	{
		free_hexp(actcfgbuf);
		return -1;	
	}
	pos += 2;

	pACP->name_len = pConf[pos++];
	memcpy(pACP->name, pConf+pos, pACP->name_len);
	pACP->name[pACP->name_len] = 0;
	pos += pACP->name_len;
	pACP->rpwrs_len = pConf[pos++];
	for (i = 0; i < pACP->rpwrs_len; ++i)
	{
		pACP->rpwrs[i] = GetNumU16(pConf+pos);
		pos += 2;
	}
	pACP->region = pConf[pos++];
	pACP->hoptab_len = pConf[pos++];
	for (i = 0; i < pACP->hoptab_len; ++i)
	{
		pACP->frepoints[i] = GetNumU32(pConf+pos);
		pos += 4;
	}
	pACP->gen2session = pConf[pos++];
	pACP->gen2q = pConf[pos++];
	pACP->is_tagdata_uni_byant = pConf[pos++];
	pACP->is_tagdata_uni_bybank = pConf[pos++];
	pACP->is_record_max_rssi = pConf[pos++];
	
	pos += 3;
	pACP->invants_len = pConf[pos++];
	for (i = 0; i < pACP->invants_len; ++i)
		pACP->inv_ants[i] = pConf[pos++];
	
	pACP->heart_beat_cylce = GetNumU16(pConf+pos);
	pos += 2;
	
	pACP->is_tagfilter = pConf[pos++];
	if (pACP->is_tagfilter == 1)
	{
		pACP->tag_filter.bank = pConf[pos++];
		pACP->tag_filter.start_bit = GetNumU16(pConf+pos);
		pos += 2;
		pACP->tag_filter.mask_len = GetNumU16(pConf+pos);
		pos += 2;
		{
			int maskbytecnt = (pACP->tag_filter.mask_len % 8 == 0) ? (pACP->tag_filter.mask_len / 8) : 
				((pACP->tag_filter.mask_len / 8) + 1);
			memcpy(pACP->tag_filter.mask, pConf+pos, maskbytecnt);
			pos += maskbytecnt;
		}
		pACP->tag_filter.is_match = pConf[pos++];
	}
	
	pACP->is_inv_bank_read = pConf[pos++];
	if (pACP->is_inv_bank_read == 1)
	{
		pACP->inv_bank_read.bank = pConf[pos++];
		pACP->inv_bank_read.start_block = pConf[pos++];
		pACP->inv_bank_read.blockcnt = pConf[pos++];
		memcpy(pACP->inv_bank_read.pwd, pConf+pos, 4);
		pos += 4;
	}
	memcpy(pACP->upload_ip, pConf+pos, 4);
	pos += 4;
	pACP->upload_port = GetNumU16(pConf+pos);
	pos += 2;
	pACP->conn_mode = pConf[pos++];
	pACP->ack_client_mode = pConf[pos++];
	
	pACP->data_aggr_mode = pConf[pos++];
	if (pACP->data_aggr_mode == 1)
	{
		pACP->data_aggr_duration = GetNumU32(pConf+pos);
		pos += 4;
	}
	
	pACP->is_gpi_trigger = pConf[pos++];
	if (pACP->is_gpi_trigger == 1)
	{
		pACP->gpi_trigger_mode = pConf[pos++];
		
		pACP->gpi_trigger1.gpi_count = pConf[pos++];
		for (i = 0; i < pACP->gpi_trigger1.gpi_count; ++i)
		{
			pACP->gpi_trigger1.gpi_ids[i] = pConf[pos++];
			pACP->gpi_trigger1.gpi_states[i] = pConf[pos++];
		}
		
		if (pACP->gpi_trigger_mode == 1 || pACP->gpi_trigger_mode == 3
			|| pACP->gpi_trigger_mode == 4)
		{
			pACP->gpi_trigger2.gpi_count = pConf[pos++];
			for (i = 0; i < pACP->gpi_trigger2.gpi_count; ++i)
			{
				pACP->gpi_trigger2.gpi_ids[i] = pConf[pos++];
				pACP->gpi_trigger2.gpi_states[i] = pConf[pos++];
			}
		}
		
		pACP->gpi_read_timeout = GetNumU32(pConf+pos);
		pos += 4;			

	}

	pACP->gpo_init_states = pConf[pos++];
	pACP->loc_gpo_act_count = pConf[pos++];
	for (i = 0; i < pACP->loc_gpo_act_count; ++i)
	{
		pACP->lga_gpo_id[i] = pConf[pos++];
		pACP->lga_gpo_states[i] = pConf[pos++];
		pACP->lga_gpo_durs[i] = GetNumU32(pConf+pos);
		pos += 4;
	}
	
	pACP->event_count = pConf[pos++];
	for (i = 0; i < pACP->event_count; ++i)
		pACP->events[i] = pConf[pos++];
	
	pACP->inv_cycle = GetNumU16(pConf+pos);
	pos += 2;
	
	pACP->interval_cycle = GetNumU16(pConf+pos);
	pos += 2;
	pACP->max_rec_databytes_length = pConf[pos++];
	pACP->cusparam_len = pConf[pos++];
	memcpy(pACP->cusparam, pConf+pos, pACP->cusparam_len);
	pACP->cusparam[pACP->cusparam_len] = 0;
	
	free_hexp(actcfgbuf);
	return 0;
}
int TestFwType(void)
{
//	int i;
	unsigned char tmpbuffer[20];
	flash_bytes_read(ActiveModeConfig_Addr, tmpbuffer, 20);
/*	
	TRACE("TestFwType:");
	for (i = 0; i < 20; ++i)
		TRACE("%02X ", tmpbuffer[i]);
	TRACE("\n");
	*/
	if (memcmp(tmpbuffer, ACPMagicStr, 15) == 0)
		return 1;
	else
		return 0;
}

WorkMode_Code TestFwType_ex(void)
{
//	int i;
	WorkMode_Code wmc;
	unsigned char tmpbuffer[20];
	flash_bytes_read(ActiveModeConfig_Addr, tmpbuffer, 20);
	get_workmode_params(&wmc);
	TRACE("TestFwType_ex wmc:%d\n", wmc);
/*	
	TRACE("TestFwType:");
	for (i = 0; i < 20; ++i)
		TRACE("%02X ", tmpbuffer[i]);
	TRACE("\n");
	*/
	if (memcmp(tmpbuffer, ACPMagicStr, 15) == 0)
		return WorkMode_ActVer_1;
	else if (memcmp(tmpbuffer, ActModeCfg_Marker, 15) == 0 && 
		wmc == WorkMode_ActVer_2)
		return WorkMode_ActVer_2;
	else
		return WorkMode_Passive;
}

void dump_active_mode_config(AppCustomParams *pACP)
{
#ifdef _DEBUG
	int i;
	TRACE("DumpAppCustomParams start--------------------\n");
	TRACE("name:%s\n", pACP->name);
	
	TRACE("rpwrs_len:%d\n", pACP->rpwrs_len);
	for (i = 0; i < pACP->rpwrs_len; ++i)
		TRACE("%d, rpwr:%d\n", i+1, pACP->rpwrs[i]);
	TRACE("region:%d\n", pACP->region);
	TRACE("hoptable cnt:%d\n", pACP->hoptab_len);
	for (i = 0; i < pACP->hoptab_len; ++i)
		TRACE("%d ", pACP->frepoints[i]);
	
	TRACE("\ngen2session:%d\n", pACP->gen2session);
	TRACE("gen2q:%d\n", pACP->gen2q);
	TRACE("is_tagdata_uni_byant:%d\n", pACP->is_tagdata_uni_byant);
	TRACE("is_tagdata_uni_bybank:%d\n", pACP->is_tagdata_uni_bybank);
	TRACE("is_record_max_rssi:%d\n", pACP->is_record_max_rssi);
	
	TRACE("invants cnt:%d\n", pACP->invants_len);
	for (i = 0; i < pACP->invants_len; ++i)
		TRACE("%d ", pACP->inv_ants[i]);
	
	TRACE("\nheart_beat_cylce:%d\n", pACP->heart_beat_cylce);
	
	TRACE("\nis_tagfilter:%d\n", pACP->is_tagfilter);
	if (pACP->is_tagfilter == 1)
	{
		TRACE("bank:%d, start_bit:%d, is_match:%d, mask_len:%d, mask:", pACP->tag_filter.bank, 
			pACP->tag_filter.start_bit, pACP->tag_filter.is_match, pACP->tag_filter.mask_len);
		{
			int maskbytecnt = (pACP->tag_filter.mask_len % 8 == 0) ? (pACP->tag_filter.mask_len / 8) : 
				((pACP->tag_filter.mask_len / 8) + 1); 
			for (i = 0; i < maskbytecnt; ++i)
				TRACE("%02X ", pACP->tag_filter.mask[i]);
		}
	}
	TRACE("\nis_inv_bank_read:%d\n", pACP->is_inv_bank_read);
	if (pACP->is_inv_bank_read == 1)
	{
		TRACE("bank:%d, start_block:%d, blockcnt:%d, pwd:%08X\n", 
			pACP->inv_bank_read.bank, pACP->inv_bank_read.start_block, pACP->inv_bank_read.blockcnt,
			GetNumU32(pACP->inv_bank_read.pwd));
	}
	
	TRACE("upload_ip:%d.%d.%d.%d\n", pACP->upload_ip[0], pACP->upload_ip[1], 
		pACP->upload_ip[2], pACP->upload_ip[3]);

	TRACE("upload_port:%d\n", pACP->upload_port);
	TRACE("conn_mode:%d\n", pACP->conn_mode);
	TRACE("ack_client_mode:%d\n", pACP->ack_client_mode);
	
	TRACE("data_aggr_mode:%d\n", pACP->data_aggr_mode);
	if (pACP->data_aggr_mode == 1)
		TRACE("data_aggr_duration:%d\n", pACP->data_aggr_duration);
	
	TRACE("is_gpi_trigger:%d\n", pACP->is_gpi_trigger);
	if (pACP->is_gpi_trigger == 1)
	{
		TRACE("gpi_trigger_mode:%d\n", pACP->gpi_trigger_mode);
		TRACE("gpi_trigger1:\n");
		for (i = 0; i < pACP->gpi_trigger1.gpi_count; ++i)
			TRACE("    gpi_id:%d, gpi_state:%d\n",pACP->gpi_trigger1.gpi_ids[i], pACP->gpi_trigger1.gpi_states[i]);
		
		if (pACP->gpi_trigger_mode == 1 || pACP->gpi_trigger_mode == 3 || 
			pACP->gpi_trigger_mode == 4)
		{
			TRACE("gpi_trigger2:\n");
			for (i = 0; i < pACP->gpi_trigger2.gpi_count; ++i)
				TRACE("    gpi_id:%d, gpi_state:%d\n",pACP->gpi_trigger2.gpi_ids[i], pACP->gpi_trigger2.gpi_states[i]);
		}
		TRACE("gpi_read_timeout:%d\n", pACP->gpi_read_timeout);		
	}
	
	TRACE("gpo init state:%02X\n", pACP->gpo_init_states);
	TRACE("local gpo action count:%d\n", pACP->loc_gpo_act_count);
	for (i = 0; i < pACP->loc_gpo_act_count; ++i)
	{
		TRACE("gpo%d state:%d dur:%d", pACP->lga_gpo_id[i], 
			pACP->lga_gpo_states[i], pACP->lga_gpo_durs[i]);
		TRACE("--");
	}
	TRACE("\n");
	
	TRACE("event_count:%d\n", pACP->event_count);
	for (i = 0; i < pACP->event_count; ++i)
		printf("%d ", pACP->events[i]);
	
	TRACE("\ninv_cycle:%d\n", pACP->inv_cycle);
	TRACE("interval_cycle:%d\n", pACP->interval_cycle);
	TRACE("max_rec_databytes_length:%d\n", pACP->max_rec_databytes_length);
	TRACE("cusparam:%s", pACP->cusparam);

	TRACE("\nDumpAppCustomParams end----------------------\n");
#endif
}

void setdef_active_mode_config(AppCustomParams *pACP)
{
	memset(pACP, 0, sizeof(AppCustomParams));
	strcpy((char*)pACP->name, "arm7Reader");
	pACP->name_len = strlen("arm7Reader");
	pACP->data_aggr_duration = 2000;
	pACP->gen2q = 255;
	pACP->heart_beat_cylce = 10;
	pACP->interval_cycle = 10;
	pACP->inv_cycle = 150;
	pACP->region = 1;
	pACP->max_rec_databytes_length = 32;
	pACP->upload_ip[0] = 169;
	pACP->upload_ip[1] = 169;
	pACP->upload_ip[2] = 169;
	pACP->upload_ip[3] = 169;
	pACP->upload_port = 12345;
	pACP->loc_gpo_act_count = 0;
	pACP->gpo_init_states = 0x0F;
}

void AppCustomParams_To_static_settings(AppCustomParams *pAppCusPara, 
	ReaderStaticSettings_ST *static_set)
{
	int i;
	static_set->protocol.gen2.session = pAppCusPara->gen2session;
	static_set->rf.region = pAppCusPara->region;
	static_set->app_init.max_tb_rec_len = pAppCusPara->max_rec_databytes_length;
	
	static_set->tagops_param.inventory.ants_cnt = pAppCusPara->invants_len;
	for (i = 0; i < pAppCusPara->invants_len; ++i)
		static_set->tagops_param.inventory.ants[i] = pAppCusPara->inv_ants[i];

	static_set->tagops_param.inventory.cycle = pAppCusPara->inv_cycle;
	static_set->tagops_param.inventory.interval = pAppCusPara->interval_cycle;
	static_set->tagops_param.inventory.inv_mode = -1;
	
	static_set->tagops_param.tagfilter.is_tagfilter = pAppCusPara->is_tagfilter;
	if (pAppCusPara->is_tagfilter == 1)
	{
		static_set->tagops_param.tagfilter.bank = pAppCusPara->tag_filter.bank;
		static_set->tagops_param.tagfilter.mask_len = pAppCusPara->tag_filter.mask_len;
		static_set->tagops_param.tagfilter.match = pAppCusPara->tag_filter.is_match;
		memcpy(static_set->tagops_param.tagfilter.mask, pAppCusPara->tag_filter.mask, 64);
		static_set->tagops_param.tagfilter.start = pAppCusPara->tag_filter.start_bit;
	}
	
	static_set->tagops_param.bankdata.is_bankdata = pAppCusPara->is_inv_bank_read;
	if ( pAppCusPara->is_inv_bank_read == 1)
	{
		static_set->tagops_param.bankdata.bank = pAppCusPara->inv_bank_read.bank;
		static_set->tagops_param.bankdata.blkcnt = pAppCusPara->inv_bank_read.blockcnt;
		static_set->tagops_param.bankdata.start = pAppCusPara->inv_bank_read.start_block;
		memcpy(static_set->tagops_param.accessop.aespwd, pAppCusPara->inv_bank_read.pwd, 4);
	}
	else
		SetNumU32(static_set->tagops_param.accessop.aespwd, 0);
	
	if (pAppCusPara->rpwrs_len == 0)
	{
		for (i = 0; i < pAppCusPara->rpwrs_len; ++i)
		{
			static_set->rf.tx_powers[i].read_power = -1;
			static_set->rf.tx_powers[i].write_power = -1;
		}
	}
	else
	{
		if (pAppCusPara->rpwrs_len == 1)
		{
			for (i = 0; i < 16; ++i)
			{
				static_set->rf.tx_powers[i].read_power = pAppCusPara->rpwrs[0];
				static_set->rf.tx_powers[i].write_power = pAppCusPara->rpwrs[0];				
			}
		}
		else
		{
			for (i = 0; i < pAppCusPara->rpwrs_len; ++i)
			{
				static_set->rf.tx_powers[i].read_power = pAppCusPara->rpwrs[i];
				static_set->rf.tx_powers[i].write_power = pAppCusPara->rpwrs[i];
			}
		}
	}
	
	static_set->tag_data.unique_by_antenna = pAppCusPara->is_tagdata_uni_byant;
	static_set->tag_data.unique_by_bank_data = pAppCusPara->is_tagdata_uni_bybank;
	static_set->tag_data.record_highest_rssi = pAppCusPara->is_record_max_rssi;
	
	if (pAppCusPara->gpo_init_states > 0)
		static_set->gpos[i] = ((pAppCusPara->gpo_init_states >> i) & 0x1) + 1;
	
	if (pAppCusPara->upload_ip[0] == 0)
	{
		static_set->uart1.baud = (pAppCusPara->upload_ip[1] << 16) | 
			(pAppCusPara->upload_ip[2] << 8) | pAppCusPara->upload_ip[3];
		static_set->uart1.data_bits = 0;
		static_set->uart1.stop_bits = 0;
		static_set->uart1.parity = 0;
		static_set->uart1.flow_ctrl = 0;
		static_set->uart1.type = 0;
	}
}


void setdef_rdr_static_settings(ReaderStaticSettings_ST *static_set)
{
	int i;
	
	static_set->is_ethernet = 0;
	static_set->uart_ex_type = 0;
	
	static_set->app_init.usb_type = rdr_st_set_usb_type_HidCdc;
	static_set->app_init.max_tb_rec_len = MaxTbItemSize;
	static_set->app_init.evt_que_len = DefEvtQueSize;
	static_set->reboot = 1;
	
	static_set->tagops_param.inventory.ants_cnt = 0;
	static_set->tagops_param.inventory.cycle = 150;
	static_set->tagops_param.inventory.interval = 10;
	static_set->tagops_param.inventory.inv_mode = 0;
	static_set->tagops_param.tagfilter.is_tagfilter = 0;
	static_set->tagops_param.bankdata.is_bankdata = 0;
	static_set->tagops_param.accessop.ant = 0;
	static_set->tagops_param.accessop.timeout = 1000;
	SetNumU32(static_set->tagops_param.accessop.aespwd, 0);
	
	static_set->tagops_param.mb_sinv_tag_fmt = 0x01;
	
	static_set->protocol.gen2.session = -1;
	static_set->protocol.gen2.q = -2;
	static_set->protocol.gen2.target = -1;
	static_set->protocol.gen2.profile = -1;
	static_set->rf.ant_max_dwell_time = 0;
	static_set->rf.hop_mode = -1;
	static_set->rf.region = -1;
	static_set->rf.hop_table_cnt = 0;
	for (i = 0; i < 16; ++i)
	{
		static_set->rf.tx_powers[i].read_power = -2;
		static_set->rf.tx_powers[i].write_power = -2;
	}
	static_set->tag_data.unique_by_antenna = -1;
	static_set->tag_data.unique_by_bank_data = -1;
	static_set->tag_data.record_highest_rssi = -1;
	
	for (i = 0; i < 5; ++i)
		static_set->gpos[i] = 0;
	

	static_set->uart1.baud = 115200;
	static_set->uart1.data_bits = 0;
	static_set->uart1.parity = 0;
	static_set->uart1.type = 0;
	static_set->uart1.address = 2;
	static_set->uart1.stop_bits = 0;
	static_set->uart1.flow_ctrl = 0;
	
	static_set->uart2.baud = 115200;
	static_set->uart2.data_bits = 0;
	static_set->uart2.parity = 0;
	static_set->uart2.type = 1;
	static_set->uart2.address = 2;
	static_set->uart2.stop_bits = 0;
	static_set->uart2.flow_ctrl = 0;
}

void dump_static_settings(ReaderStaticSettings_ST *static_set)
{
#ifdef _DEBUG
	int i;
	TRACE("----------------------- dump_static_settings start\n");
	TRACE("reboot:%d\n", static_set->reboot);
	
	TRACE("inventory.cycle:%d\n", static_set->tagops_param.inventory.cycle);
	TRACE("inventory.interval:%d\n", static_set->tagops_param.inventory.interval);
	TRACE("inventory.inv_mode:%d\n", static_set->tagops_param.inventory.inv_mode);
	TRACE("inventory.ants:");
	for (i = 0; i < static_set->tagops_param.inventory.ants_cnt; ++i)
			printf("%d ", static_set->tagops_param.inventory.ants[i]);
	TRACE("\n");
	
	if (static_set->tagops_param.tagfilter.is_tagfilter == 1)
	{
		TRACE("tagfilter.bank:%d\n", static_set->tagops_param.tagfilter.bank);
		TRACE("tagfilter.start:%d\n", static_set->tagops_param.tagfilter.start);
		TRACE("tagfilter.match:%d\n", static_set->tagops_param.tagfilter.match);
		TRACE("tagfilter.mask_len:%d\n", static_set->tagops_param.tagfilter.mask_len);
		TRACE("tagfilter.mask:");
		for (i = 0; i < static_set->tagops_param.tagfilter.mask_len/8+1; ++i)
			TRACE("%02X ", static_set->tagops_param.tagfilter.mask[i]);
		TRACE("\n");
		TRACE("\n");
	}
	
	if (static_set->tagops_param.bankdata.is_bankdata == 1)
	{
		TRACE("bankdata.bank:%d\n", static_set->tagops_param.bankdata.bank);
		TRACE("bankdata.start:%d\n", static_set->tagops_param.bankdata.start);
		TRACE("bankdata.blkcnt:%d\n", static_set->tagops_param.bankdata.blkcnt);
//		printf("bankdata.pwd:%08X\n", GetNumU32(rt_set->bankdata.pwd));
	}

	TRACE("accessop: ant:%d, timeout:%d, pwd:%04X\n", static_set->tagops_param.accessop.ant, 
		static_set->tagops_param.accessop.timeout, 
		GetNumU32(static_set->tagops_param.accessop.aespwd));
	
	TRACE("tagops_param.mb_sinv_tag_fmt:%04X\n", static_set->tagops_param.mb_sinv_tag_fmt);
	
	TRACE("app_init.usb_type:%d\n", static_set->app_init.usb_type);
	TRACE("app_init.max_tb_rec_len:%d\n", static_set->app_init.max_tb_rec_len);
	TRACE("app_init.evt_que_len:%d\n", static_set->app_init.evt_que_len);
	
	TRACE("protocol.gen2.session:%d\n", static_set->protocol.gen2.session);
	TRACE("protocol.gen2.q:%d\n", static_set->protocol.gen2.q);
	TRACE("protocol.gen2.target:%d\n", static_set->protocol.gen2.target);
	TRACE("protocol.gen2.profile:%d\n", static_set->protocol.gen2.profile);	
	
	TRACE("rf.ant_max_dwell_time:%d\n", static_set->rf.ant_max_dwell_time);
	TRACE("rf.hop_mode:%d\n", static_set->rf.hop_mode);
	TRACE("rf.region:%d\n", static_set->rf.region);
	if (static_set->rf.hop_table_cnt != 0)
	{		
		TRACE("hoptable:");
		for (i = 0; i < static_set->rf.hop_table_cnt; ++i)
		{
			TRACE("%d ", static_set->rf.hop_table[i]);
		}
		TRACE("\n");
	}

	TRACE("tx_powers:");
	for (i = 0; i < 16; ++i)
	{
		if (static_set->rf.tx_powers[i].read_power != 0)
		{
			TRACE("%d %d ", i+1, static_set->rf.tx_powers[i].read_power);
			TRACE("%d, ", static_set->rf.tx_powers[i].write_power);
		}
	}
	TRACE("\n");

	
	TRACE("tag_data.unique_by_antenna:%d\n", static_set->tag_data.unique_by_antenna);
	TRACE("tag_data.unique_by_bank_data:%d\n", static_set->tag_data.unique_by_bank_data);
	TRACE("tag_data.record_highest_rssi:%d\n", static_set->tag_data.record_highest_rssi);
	
	TRACE("gpos:");
	for (i = 0; i < 5; ++i)
		TRACE("%d %d, ", i+1, static_set->gpos[i]);		
	TRACE("\n");
	
	
	TRACE("uart1.address:%d\n", static_set->uart1.address);
	TRACE("uart1.baud:%d\n", static_set->uart1.baud);
	TRACE("uart1.data_bits:%d\n", static_set->uart1.data_bits);
	TRACE("uart1.stop_bits:%d\n", static_set->uart1.stop_bits);
	TRACE("uart1.flow_ctrl:%d\n", static_set->uart1.flow_ctrl);
	TRACE("uart1.parity:%d\n", static_set->uart1.parity);
	TRACE("uart1.type:%d\n", static_set->uart1.type);

	TRACE("uart2.address:%d\n", static_set->uart2.address);
	TRACE("uart2.baud:%d\n", static_set->uart2.baud);
	TRACE("uart2.data_bits:%d\n", static_set->uart2.data_bits);
	TRACE("uart2.stop_bits:%d\n", static_set->uart2.stop_bits);
	TRACE("uart2.flow_ctrl:%d\n", static_set->uart2.flow_ctrl);
	TRACE("uart2.parity:%d\n", static_set->uart2.parity);
	TRACE("uart2.type:%d\n", static_set->uart2.type);
	
	if (static_set->is_ethernet == 1)
		dump_network_config(&static_set->ethernet);
	
	if (static_set->uart_ex_type == Uart_Ex_Wlan)
		dump_wlan_config(&static_set->uart_ex.wlan);
	
	if (static_set->uart_ex_type == Uart_Ex_Bluetooth)
		dump_bluetooth_config(&static_set->uart_ex.ble);

	if (static_set->uart_ex_type == Uart_Ex_4G)
		dump_monet_config(&static_set->uart_ex.monet);
	
	TRACE("----------------------- dump_static_settings end\n");
#endif
}


void Str2Binary(const char *buf,int len,unsigned char *binarybuf);
void dump_runtime_settings(ReaderRunTimeSettings_ST *rt_set)
{
#ifdef _DEBUG
	int i;

	TRACE("---------------------- dump_runtime_settings start\n");
	TRACE("glob_params.hb_cylce:%d\n", rt_set->glob_params.hb_cylce);
	TRACE("glob_params.s_buf_size:%d\n", rt_set->glob_params.s_buf_size);
	TRACE("glob_params.name:%s\n", (char *)rt_set->glob_params.name);
	TRACE("\n");
	/*
	printf("inventory.cycle:%d\n", rt_set->inventory.cycle);
	printf("inventory.interval:%d\n", rt_set->inventory.interval);
	printf("inventory.inv_mode:%d\n", rt_set->inventory.inv_mode);
	printf("inventory.ants:");
	for (i = 0; i < 16; ++i)
	{
		if (rt_set->inventory.ants[i] != 0)
			printf("%d ", i+1);
	}
	printf("\n");
	
	if (rt_set->tagfilter.is_tagfilter == 1)
	{
		printf("tagfilter.bank:%d\n", rt_set->tagfilter.bank);
		printf("tagfilter.start:%d\n", rt_set->tagfilter.start);
		printf("tagfilter.match:%d\n", rt_set->tagfilter.match);
		printf("tagfilter.mask_len:%d\n", rt_set->tagfilter.mask_len);
		printf("tagfilter.mask:");
		for (i = 0; i < rt_set->tagfilter.mask_len/8+1; ++i)
			printf("%02X ", rt_set->tagfilter.mask[i]);
		printf("\n");
		printf("\n");
	}
	
	if (rt_set->bankdata.is_bankdata == 1)
	{
		printf("bankdata.bank:%d\n", rt_set->bankdata.bank);
		printf("bankdata.start:%d\n", rt_set->bankdata.start);
		printf("bankdata.blkcnt:%d\n", rt_set->bankdata.blkcnt);
		printf("bankdata.pwd:%08X\n", GetNumU32(rt_set->bankdata.pwd));
	}
	*/
	TRACE("upload.data_aggr.mode:%d\n", rt_set->upload.data_aggr.mode);
	TRACE("upload.data_aggr.timeval:%d\n", rt_set->upload.data_aggr.timeval);
	TRACE("upload.hw_inf:%d\n", rt_set->upload.hw_inf);
	TRACE("upload.sw_potl:%d\n", rt_set->upload.sw_potl);
	
	if (rt_set->upload.hw_inf == Upload_Inf_Ethernet || 
		rt_set->upload.hw_inf == Upload_Inf_4G || 
		rt_set->upload.hw_inf == Upload_Inf_Wifi)
	{
		if (rt_set->upload.sw_potl == Upload_Trans_Potl_Tcp)
		{
			TRACE("upload.sw_potl_params.tcp.ser_ip:%s\n", rt_set->upload.sw_potl_params.tcp.ser_ip);
			TRACE("upload.sw_potl_params.tcp.ser_port:%d\n", rt_set->upload.sw_potl_params.tcp.ser_port);
		}
		else if (rt_set->upload.sw_potl == Upload_Trans_Potl_Http)
		{
			TRACE("upload.sw_potl_params.http:\n");
			TRACE("url:%s\n", rt_set->upload.sw_potl_params.http.url);
		}
		else if (rt_set->upload.sw_potl == Upload_Trans_Potl_Mqtt)
		{
			TRACE("upload.sw_potl_params.mqtt:\n");
			TRACE("host:%s\n", rt_set->upload.sw_potl_params.mqtt.host);
			TRACE("port:%d\n", rt_set->upload.sw_potl_params.mqtt.port);
			TRACE("user:%s\n", rt_set->upload.sw_potl_params.mqtt.user);
			TRACE("pwd:%s\n", rt_set->upload.sw_potl_params.mqtt.pwd);
			TRACE("kal_time:%d\n", rt_set->upload.sw_potl_params.mqtt.kal_time);
			TRACE("sub_b_topic:%s\n", rt_set->upload.sw_potl_params.mqtt.sub_b_topic);
			TRACE("sub_b_qos:%d\n", rt_set->upload.sw_potl_params.mqtt.sub_b_qos);
			TRACE("sub_u_topic:%s\n", rt_set->upload.sw_potl_params.mqtt.sub_u_topic);
			TRACE("sub_u_qos:%d\n", rt_set->upload.sw_potl_params.mqtt.sub_u_qos);
			TRACE("pub_topic:%s\n", rt_set->upload.sw_potl_params.mqtt.pub_topic);
			TRACE("pub_qos:%d\n", rt_set->upload.sw_potl_params.mqtt.pub_qos);
			TRACE("tls:%d\n", rt_set->upload.sw_potl_params.mqtt.tls);			
		}
	}
	else if (rt_set->upload.hw_inf == Upload_Inf_Wiegand)
	{
		TRACE("wiegand.type:%d\n", rt_set->upload.sw_potl_params.wiegand.type);
		TRACE("wiegand.pls_width:%d\n", rt_set->upload.sw_potl_params.wiegand.pls_width);
		TRACE("wiegand.pls_interval:%d\n", rt_set->upload.sw_potl_params.wiegand.pls_interval);
		TRACE("wiegand.data_interval:%d\n", rt_set->upload.sw_potl_params.wiegand.data_interval);
		TRACE("wiegand.bytes_order:%d\n", rt_set->upload.sw_potl_params.wiegand.bytes_order);		
	}
	
	TRACE("upload.client_ack:%d\n", rt_set->upload.client_ack);
	TRACE("upload.crc_enable:%d\n", rt_set->upload.crc_enable);
	TRACE("upload.recv_timeout:%d\n", rt_set->upload.recv_timeout);
	TRACE("upload.clr_r_buf_time:%d\n", rt_set->upload.clr_r_buf_time);
	TRACE("\n");
	
	if (rt_set->gpi_trigger.is_gpi_trigger == 1)
	{
		TRACE("gpi_trigger.mode:%d\n", rt_set->gpi_trigger.mode);
		TRACE("gpi_trigger.timeval:%d\n", rt_set->gpi_trigger.timeval);
		if (rt_set->gpi_trigger.mode == 5)
			TRACE("gpi_trigger.timeval2:%d\n", rt_set->gpi_trigger.timeval2);
		TRACE("gpi_trigger.cond_1:");
		for (i = 0; i < rt_set->gpi_trigger.cond_1.count; ++i)
			TRACE("%d %d, ", rt_set->gpi_trigger.cond_1.ids[i], 
				rt_set->gpi_trigger.cond_1.states[i]);
		TRACE("\n");
		if (rt_set->gpi_trigger.mode != 2)
		{
			TRACE("gpi_trigger.cond_2:");
			for (i = 0; i < rt_set->gpi_trigger.cond_2.count; ++i)
				TRACE("%d %d,", rt_set->gpi_trigger.cond_2.ids[i], 
					rt_set->gpi_trigger.cond_2.states[i]);
			TRACE("\n");
		}
		TRACE("\n");
	}
	
	if (rt_set->gpo_act.count > 0)
	{
		TRACE("gpo_act:");
		for (i = 0; i < rt_set->gpo_act.count; ++i)
			TRACE("%d %d %d,", rt_set->gpo_act.ids[i], 
				rt_set->gpo_act.states[i], rt_set->gpo_act.durs[i]);
		TRACE("\n");
	}
	
	if (rt_set->events.count > 0)
	{
		TRACE("events:");
		for (i = 0; i < rt_set->events.count; ++i)
			printf("%d ", rt_set->events.ids[i]);
		TRACE("\n");
	}
	
	TRACE("tag_json_format:%04X\n", rt_set->tag_json_format);
	TRACE("cus_param:%s\n", (char *)rt_set->cus_param.param);
	
	TRACE("---------------------- dump_runtime_settings end\n");
#endif
}

int valid_runtime_settings_byobj(json_value *jvalue, 
	ReaderRunTimeSettings_ST *rt_set)
{
	json_value *obj;
	json_value *tmpobj;
	int pvalint;
	int i;
	int exflag;
	
	rt_set->reset = 0;
	PARSE_CHK_RANGE(jvalue, "reset", rt_set->reset, 0, 1, 0);
	
	if (json_getobject(jvalue, "glob_params", &obj) == 0)
	{
		PARSE_CHK_RANGE(obj, "hb_cylce", rt_set->glob_params.hb_cylce, 1, 0xffff, 0);
		
		PARSE_CHK_RANGE(obj, "s_buf_size", rt_set->glob_params.s_buf_size, 1500, 8192, 0);
		
		PARSE_CHK_STR(obj, "name", rt_set->glob_params.name, 128, 0, 0);
	}

	if (json_getobject(jvalue, "upload", &obj) == 0)
	{
		PARSE_CHK_RANGE(obj, "client_ack", rt_set->upload.client_ack, 0, 1, 0);
		
		PARSE_CHK_RANGE(obj, "crc_enable", rt_set->upload.crc_enable, 0, 1, 0);
	
		PARSE_CHK_RANGE(obj, "recv_timeout", rt_set->upload.recv_timeout, 5, 60, 0);
		
		PARSE_CHK_RANGE(obj, "clr_r_buf_time", rt_set->upload.clr_r_buf_time, 8, 30, 0);
		
		PARSE_CHK_RANGE(obj, "hw_inf", rt_set->upload.hw_inf, 1, 7, 0);
		
		if (json_getobject(obj, "data_aggr", &tmpobj) == 0)
		{
			PARSE_CHK_RANGE(tmpobj, "mode", rt_set->upload.data_aggr.mode, 1, 1, 1);
			
			PARSE_CHK_RANGE(tmpobj, "timeval", rt_set->upload.data_aggr.timeval, 50, 86400000, 1);
		}
		
		if (rt_set->upload.hw_inf == Upload_Inf_Ethernet || 
			rt_set->upload.hw_inf == Upload_Inf_4G || 
			rt_set->upload.hw_inf == Upload_Inf_Wifi)
		{
			if (json_getobject(obj, "tcp", &tmpobj) == 0)
			{
				PARSE_CHK_STR(tmpobj, "ser_ip", rt_set->upload.sw_potl_params.tcp.ser_ip, 48, 1, 0);
				PARSE_CHK_RANGE(tmpobj, "ser_port", rt_set->upload.sw_potl_params.tcp.ser_port, 100, 65535, 1);
				rt_set->upload.sw_potl = Upload_Trans_Potl_Tcp;
			}
			else if (json_getobject(obj, "http", &tmpobj) == 0)
			{
				PARSE_CHK_STR(tmpobj, "url", rt_set->upload.sw_potl_params.http.url, (MaxUploadHttpUrlLen-1), 1, 0);
				rt_set->upload.sw_potl = Upload_Trans_Potl_Http;
			}
			else if (json_getobject(obj, "mqtt", &tmpobj) == 0)
			{
				PARSE_CHK_STR(tmpobj, "host", rt_set->upload.sw_potl_params.mqtt.host, (Mqtt_MaxHostLen-1), 1, 0);
				PARSE_CHK_RANGE(tmpobj, "port", rt_set->upload.sw_potl_params.mqtt.port, 100, 65535, 1);
				PARSE_CHK_STR(tmpobj, "user", rt_set->upload.sw_potl_params.mqtt.user, (Mqtt_MaxUserLen-1), 1, 1);				
				PARSE_CHK_STR(tmpobj, "pwd", rt_set->upload.sw_potl_params.mqtt.pwd, (Mqtt_MaxPwdLen-1), 1, 1);
				PARSE_CHK_RANGE(tmpobj, "kal_time", rt_set->upload.sw_potl_params.mqtt.kal_time, 5, 86400, 1);
				PARSE_CHK_STR(tmpobj, "sub_b_topic", rt_set->upload.sw_potl_params.mqtt.sub_b_topic, (Mqtt_MaxSubBrdCTpLen-1), 1, 1);
				PARSE_CHK_STR(tmpobj, "sub_u_topic", rt_set->upload.sw_potl_params.mqtt.sub_u_topic, (Mqtt_MaxSubUniCTpLen-1), 1, 1);
				PARSE_CHK_STR(tmpobj, "pub_topic", rt_set->upload.sw_potl_params.mqtt.pub_topic, (Mqtt_MaxPubTpLen-1), 1, 1);
				PARSE_CHK_RANGE(tmpobj, "sub_b_qos", rt_set->upload.sw_potl_params.mqtt.sub_b_qos, 0, 2, 1);
				PARSE_CHK_RANGE(tmpobj, "sub_u_qos", rt_set->upload.sw_potl_params.mqtt.sub_u_qos, 0, 2, 1);
				PARSE_CHK_RANGE(tmpobj, "pub_qos", rt_set->upload.sw_potl_params.mqtt.pub_qos, 0, 2, 1);
				PARSE_CHK_RANGE(tmpobj, "tls", rt_set->upload.sw_potl_params.mqtt.tls, 0, 1, 1);
				rt_set->upload.sw_potl = Upload_Trans_Potl_Mqtt;
			}				
		}
		else if (rt_set->upload.hw_inf == Upload_Inf_Wiegand)
		{
			if (json_getobject(obj, "wiegand", &tmpobj) == 0)
			{
				PARSE_CHK_RANGE(tmpobj, "pls_width", rt_set->upload.sw_potl_params.wiegand.pls_width, 4, 20, 1);
				PARSE_CHK_RANGE(tmpobj, "pls_interval", rt_set->upload.sw_potl_params.wiegand.pls_interval, 8, 200, 1);
				PARSE_CHK_RANGE(tmpobj, "data_interval", rt_set->upload.sw_potl_params.wiegand.data_interval, 50, 1000, 1);
				PARSE_CHK_RANGE(tmpobj, "type", rt_set->upload.sw_potl_params.wiegand.type, 1, 3, 1);
				PARSE_CHK_RANGE(tmpobj, "bytes_order", rt_set->upload.sw_potl_params.wiegand.bytes_order, 0, 1, 0);
			}
			else
			{
				TRACE("wiegand not find\n");
				return -1;
			}	
		}
	}
	
	if (json_getobject(jvalue, "gpi_trigger", &obj) == 0)
	{
		PARSE_CHK_RANGE(obj, "mode", rt_set->gpi_trigger.mode, 1, 5, 1);
		
		PARSE_CHK_RANGE(obj, "cond_order", rt_set->gpi_trigger.cond_order, 1, 2, 0);
		
		if (rt_set->gpi_trigger.mode == 2 || rt_set->gpi_trigger.mode == 3 || 
			rt_set->gpi_trigger.mode == 5)
		{
			PARSE_CHK_RANGE(obj, "timeval", rt_set->gpi_trigger.timeval, 500, 86400000, 1);
		}
		else if (rt_set->gpi_trigger.mode == 1 || rt_set->gpi_trigger.mode == 4)
		{
			PARSE_CHK_RANGE(obj, "timeval", rt_set->gpi_trigger.timeval, 5, 86400000, 1);
		}
		
		if (rt_set->gpi_trigger.mode == 5)
		{
			PARSE_CHK_RANGE(obj, "timeval2", rt_set->gpi_trigger.timeval2, 5, 86400000, 1);
		}
		
		if (json_getobject(obj, "cond_1", &tmpobj) == 0)
		{
			if (tmpobj->type == json_array)
			{
				int gpicnt = tmpobj->u.array.length;
				if (gpicnt == 0 || gpicnt > 4)
				{
					TRACE("gpi_trigger.cond_1 length is invalid\n");
					return -1;
				}
				for (i = 0; i < gpicnt; ++i)
				{
					PARSE_CHK_RANGE(tmpobj->u.array.values[i], "id", rt_set->gpi_trigger.cond_1.ids[i], 1, 4, 1);

					PARSE_CHK_RANGE(tmpobj->u.array.values[i], "state", rt_set->gpi_trigger.cond_1.states[i], 0, 1, 1);				
				}			
				rt_set->gpi_trigger.cond_1.count = gpicnt;
			}
			else
			{
				TRACE("gpi_trigger.cond_1 type error\n");
				return -1;
			}
		}
		else
		{
			TRACE("gpi_trigger.cond_1 dose not find\n");
			return -1;
		}
		
		if (rt_set->gpi_trigger.mode != 2)
		{
			if (json_getobject(obj, "cond_2", &tmpobj) == 0)
			{
				if (tmpobj->type == json_array)
				{
					int gpicnt = tmpobj->u.array.length;
					if (gpicnt == 0 || gpicnt > 4)
					{
						TRACE("gpi_trigger.cond_2 length is invalid\n");
						return -1;
					}
					for (i = 0; i < gpicnt; ++i)
					{
						PARSE_CHK_RANGE(tmpobj->u.array.values[i], "id", rt_set->gpi_trigger.cond_2.ids[i], 1, 4, 1);

						PARSE_CHK_RANGE(tmpobj->u.array.values[i], "state", rt_set->gpi_trigger.cond_2.states[i], 0, 1, 1);				
					}
					rt_set->gpi_trigger.cond_2.count = gpicnt;
				}
				else
				{
					TRACE("gpi_trigger.cond_2 type error\n");
					return -1;
				}
			}
			else
			{
				TRACE("gpi_trigger.cond_2 dose not find\n");
				return -1;
			}			
		}
		
		rt_set->gpi_trigger.is_gpi_trigger = 1;
	}
	

	PARSE_CHK_STR(jvalue, "cus_param", ((char *)rt_set->cus_param.param), 128, 0, 1);
	if (exflag)
	{
		int cuslen = strlen((char *)rt_set->cus_param.param);
//		printf("1111111111111111111111 cuslen:%d\n", cuslen);
		rt_set->cus_param.len = cuslen;
	}
	
	if (json_getobject(jvalue, "gpo_act", &obj) == 0)
	{
		if (obj->type == json_array)
		{
			int gacnt = obj->u.array.length;
			if (gacnt > 5)
			{
				TRACE("gpo_act length is invalid\n");
				return -1;
			}
			for (i = 0; i < gacnt; ++i)
			{
				PARSE_CHK_RANGE(obj->u.array.values[i], "id", rt_set->gpo_act.ids[i], 1, 5, 1);

				PARSE_CHK_RANGE(obj->u.array.values[i], "state", rt_set->gpo_act.states[i], 0, 1, 1);

				PARSE_CHK_RANGE(obj->u.array.values[i], "dur", rt_set->gpo_act.durs[i], 5, 86400000, 1);				
			}
			rt_set->gpo_act.count = gacnt;
		}
		else
		{
			TRACE("gpo_act type error\n");
			return -1;
		}
	}
	
	if (json_getobject(jvalue, "tag_json_format", &obj) == 0)
	{
		if (obj->type == json_array)
		{
			int tjfcnt = obj->u.array.length;
			char tmpselbuf[8];
			int tjfid;
			
			if (tjfcnt > 9)
			{
				TRACE("tag_json_format obj length is invalid\n");
				return -1;
			}
			rt_set->tag_json_format = 0;
			for (i = 0; i < tjfcnt; ++i)
			{
				sprintf(tmpselbuf, "[%d]", i);
				if (json_getint(obj, tmpselbuf, &tjfid) != 0)
				{
					TRACE("tag_json_format item is invalid\n");
					return -1;
				}
				rt_set->tag_json_format |= 1 << tjfid;
			}
		}
		else
		{
			TRACE("tag_json_format obj type is invalid\n");
			return -1;
		}				
	}
	
	if (json_getobject(jvalue, "events", &obj) == 0)
	{
		if (obj->type == json_array)
		{
			int ecnt = obj->u.array.length;
			char tmpselbuf[8];
			int eid;
			
			if (ecnt > 10)
			{
				TRACE("events obj length is invalid\n");
				return -1;
			}
			for (i = 0; i < ecnt; ++i)
			{
				sprintf(tmpselbuf, "[%d]", i);
				if (json_getint(obj, tmpselbuf, &eid) != 0)
				{
					TRACE("events item is invalid\n");
					return -1;
				}		
				rt_set->events.ids[i] = eid;
			}
			rt_set->events.count = ecnt;
		}
		else
		{
			TRACE("events obj type is invalid\n");
			return -1;
		}		
	}
	return 0;	
}

int valid_runtime_settings(char *buf, int blen, ReaderRunTimeSettings_ST *rt_set)
{
	json_value *jvalue;
	
	jvalue = json_parse(buf, blen);
	if (jvalue == NULL)
	{
		TRACE("valid_runtime_settings jvalue == NULL\n");
		return -1;
	}
	return valid_runtime_settings_byobj(jvalue, rt_set);
}
int check_runtime_settings(ReaderRunTimeSettings_ST *rt_set)
{
	if (rt_set->upload.hw_inf == 0)					
		return -1;
	else
	{
		if (Upload_Inf_Ethernet == rt_set->upload.hw_inf)
		{
			if (get_spi_ex_dev() != Spi_Ex_Ethernet)
				return -1;
		}
		else if (Upload_Inf_4G == rt_set->upload.hw_inf)
		{
			if (get_uart_ex_dev() != Uart_Ex_4G)
				return -1;
		}
		else if (Upload_Inf_Wifi == rt_set->upload.hw_inf)
		{
			if (get_uart_ex_dev() != Uart_Ex_Wlan)
				return -1;
		}

		if (rt_set->upload.hw_inf == Upload_Inf_Ethernet || 
					rt_set->upload.hw_inf == Upload_Inf_4G || 
					rt_set->upload.hw_inf == Upload_Inf_Wifi)
		{
			if (rt_set->upload.sw_potl == Upload_Trans_Potl_None)
				return -1;
		}
	}
	
	return 0;
}

int cmd_config_runtime_settings(json_value *jvalue, 
	char *cfg_name, char *buf, int blen)
{
	ReaderRunTimeSettings_ST *rrtsettings;
	int validret;
	
	rrtsettings = malloc_hexp(sizeof(ReaderRunTimeSettings_ST));
	if (jvalue == NULL)
		validret = valid_runtime_settings(buf, blen, rrtsettings);
	else
		validret = valid_runtime_settings_byobj(jvalue, rrtsettings);
	
	if (validret == 0)
	{
		setdef_rdr_runtime_settings(rrtsettings);
		if (rrtsettings->reset == 0)
			get_rdr_runtime_settings(rrtsettings);
		if (jvalue == NULL)
			validret = valid_runtime_settings(buf, blen, rrtsettings);
		else
		{
			json_value *obj1;
			json_value *obj2;
			obj1 = json_parse(buf, blen);
			if (obj1 != NULL)
			{
				if (json_getobject(obj1, cfg_name, &obj2) == 0)
					validret = valid_runtime_settings_byobj(obj2, rrtsettings);
				else
				{
					TRACE("valid_runtime_settings_byobj(obj2, rrtsettings) failed\n");
					validret = -1;
				}
			}
			else
			{
				TRACE("cmd_config_static_settings obj1 == NULL\n");
				validret = -1;		
			}
		}
	
		if (validret == 0)
		{
			dump_runtime_settings(rrtsettings);
			validret = check_runtime_settings(rrtsettings);
			if (validret == 0)
			{
				set_rdr_runtime_settings(rrtsettings);
				sleep_ms(200);
				set_workmode_params(WorkMode_ActVer_2);
			}
		}
	}
	
	free_hexp(rrtsettings);
	return validret;
}

void Data2BinStr(unsigned char *buf, int bitlen, char *str)
{
	*str = 0;
	int tmp;
	char tmpbuf[10];
	int i;
	for (i = 0; i < bitlen/8; ++i)
	{
		for (int j = 0; j < 8; ++j)
		{
			tmp = (buf[i] >> (7 - j)) & 0x1;
			sprintf(tmpbuf, "%d", tmp);
			strcat(str, tmpbuf);
		}
	}

	for (int c = 0; c < bitlen % 8; ++c)
	{
		tmp = (buf[i] >> (7 - c)) & 0x1;
		sprintf(tmpbuf, "%d", tmp);
		strcat(str, tmpbuf);
	}
}

void setdef_rdr_runtime_settings(ReaderRunTimeSettings_ST *rt_set)
{
//	int i;
	
	rt_set->tag_json_format = 0x0007;
	rt_set->glob_params.hb_cylce = 10;
	rt_set->glob_params.s_buf_size = 1580;
	if (get_spi_ex_dev() == Spi_Ex_Ethernet)
		sprintf(rt_set->glob_params.name, "%02X%02X%02X%02X%02X%02X", 
			gNetConf.mac[0], gNetConf.mac[1], gNetConf.mac[2], gNetConf.mac[3], 
			gNetConf.mac[4], gNetConf.mac[5]);
	else
		rt_set->glob_params.name[0] = 0;
	
//	rt_set->bankdata.is_bankdata = 0;
	
	rt_set->cus_param.param[0] = 0;
	rt_set->cus_param.len = 0;
	
	rt_set->events.count = 2;
	
	rt_set->events.ids[0] = rdr_rt_evt_TagRead;
	rt_set->events.ids[1] = rdr_rt_evt_HeartBeat;
	
	memset(&rt_set->gpi_trigger, 0, sizeof(rdr_rt_set_gpi_trigger));
	
	rt_set->gpi_trigger.cond_order = 1;
	
	rt_set->gpo_act.count = 0;
	/*
	for (i = 0; i < 16; ++i)
		rt_set->inventory.ants[i] = 0;
	rt_set->inventory.interval = 10;
	rt_set->inventory.cycle = 150;
	rt_set->inventory.inv_mode = 0;
	
	rt_set->tagfilter.is_tagfilter = 0;
	*/
	rt_set->upload.hw_inf = 0;
	rt_set->upload.sw_potl = 0;
	rt_set->upload.data_aggr.mode = 1;
	rt_set->upload.data_aggr.timeval = 2000;
	rt_set->upload.client_ack = 0;
	rt_set->upload.crc_enable = 1;
	rt_set->upload.recv_timeout = 10;
	rt_set->upload.clr_r_buf_time = 8;
//	memset(rt_set->bankdata.pwd, 0, 4);
}

void tojson_rdr_runtime_settings(ReaderRunTimeSettings_ST *rt_set, char *jstart, int *len)
{
	int i;
//	int isants = 0;
	
	sprintf(jstart, "{\"glob_params\":{\"hb_cylce\":%d,\"s_buf_size\":%d,\"name\":\"%s\"},", 
		rt_set->glob_params.hb_cylce, rt_set->glob_params.s_buf_size, rt_set->glob_params.name);

	sprintf(jstart+strlen(jstart),"\"upload\":{\"client_ack\":%d,\"crc_enable\":%d,\"recv_timeout\":%d,\"clr_r_buf_time\":%d,\"hw_inf\":%d,", 
		rt_set->upload.client_ack, rt_set->upload.crc_enable, rt_set->upload.recv_timeout, rt_set->upload.clr_r_buf_time, rt_set->upload.hw_inf);
	if (rt_set->upload.hw_inf == Upload_Inf_Ethernet || 
		rt_set->upload.hw_inf == Upload_Inf_4G || 
		rt_set->upload.hw_inf == Upload_Inf_Wifi)
	{
		if (rt_set->upload.sw_potl == Upload_Trans_Potl_Tcp)
			sprintf(jstart+strlen(jstart), "\"tcp\":{\"ser_ip\":\"%s\",\"ser_port\":%d},",
				rt_set->upload.sw_potl_params.tcp.ser_ip, rt_set->upload.sw_potl_params.tcp.ser_port);
		else if (rt_set->upload.sw_potl == Upload_Trans_Potl_Http)
			sprintf(jstart+strlen(jstart), "\"http\":{\"url\":\"%s\"},", rt_set->upload.sw_potl_params.http.url);
		else if (rt_set->upload.sw_potl == Upload_Trans_Potl_Mqtt)
		{
			sprintf(jstart+strlen(jstart), "\"mqtt\":{\"host\":\"%s\",\"port\":%d,\"kal_time\":%d,\"pub_topic\":\"%s\",\"pub_qos\":%d,", 
			rt_set->upload.sw_potl_params.mqtt.host, rt_set->upload.sw_potl_params.mqtt.port, 
			rt_set->upload.sw_potl_params.mqtt.kal_time, rt_set->upload.sw_potl_params.mqtt.pub_topic, 
			rt_set->upload.sw_potl_params.mqtt.pub_qos);
			sprintf(jstart+strlen(jstart), "\"user\":\"%s\",", rt_set->upload.sw_potl_params.mqtt.user);
			sprintf(jstart+strlen(jstart), "\"pwd\":\"%s\",", rt_set->upload.sw_potl_params.mqtt.pwd);
			sprintf(jstart+strlen(jstart), "\"sub_b_topic\":\"%s\",", rt_set->upload.sw_potl_params.mqtt.sub_b_topic);
			sprintf(jstart+strlen(jstart), "\"sub_b_qos\":%d,", rt_set->upload.sw_potl_params.mqtt.sub_b_qos);
			sprintf(jstart+strlen(jstart), "\"sub_u_topic\":\"%s\",", rt_set->upload.sw_potl_params.mqtt.sub_u_topic);			
			sprintf(jstart+strlen(jstart), "\"sub_u_qos\":%d,", rt_set->upload.sw_potl_params.mqtt.sub_u_qos);
			sprintf(jstart+strlen(jstart), "\"tls\":%d},", rt_set->upload.sw_potl_params.mqtt.tls);
		}
	}
	else if (rt_set->upload.hw_inf == Upload_Inf_Wiegand)
		sprintf(jstart+strlen(jstart), "\"wiegand\":{\"type\":%d,\"pls_width\":%d,\"pls_interval\":%d,\"data_interval\":%d,\"bytes_order\":%d},", 
			rt_set->upload.sw_potl_params.wiegand.type, rt_set->upload.sw_potl_params.wiegand.pls_width, 
			rt_set->upload.sw_potl_params.wiegand.pls_interval, rt_set->upload.sw_potl_params.wiegand.data_interval,
			rt_set->upload.sw_potl_params.wiegand.bytes_order);
	
	sprintf(jstart+strlen(jstart), "\"data_aggr\":{\"mode\":%d,\"timeval\":%d}", 
		rt_set->upload.data_aggr.mode, rt_set->upload.data_aggr.timeval);
	sprintf(jstart+strlen(jstart), "},");
	
	if (rt_set->gpi_trigger.is_gpi_trigger == 1)
	{
		if (rt_set->gpi_trigger.mode != 5)
			sprintf(jstart+strlen(jstart),"\"gpi_trigger\":{\"mode\":%d,\"cond_order\":%d,\"timeval\":%d,\"cond_1\":[", 
				rt_set->gpi_trigger.mode, rt_set->gpi_trigger.cond_order, rt_set->gpi_trigger.timeval);
		else
			sprintf(jstart+strlen(jstart),"\"gpi_trigger\":{\"mode\":%d,\"cond_order\":%d,\"timeval\":%d,\"timeval2\":%d,\"cond_1\":[", 
				rt_set->gpi_trigger.mode, rt_set->gpi_trigger.cond_order, rt_set->gpi_trigger.timeval, rt_set->gpi_trigger.timeval2);
		
		for (i = 0; i < rt_set->gpi_trigger.cond_1.count; ++i)
		{
			sprintf(jstart+strlen(jstart), "{\"id\":%d,\"state\":%d},", rt_set->gpi_trigger.cond_1.ids[i], 
				rt_set->gpi_trigger.cond_1.states[i]);
		}
		sprintf(jstart+strlen(jstart)-1, "],");
		
		if (rt_set->gpi_trigger.mode != 2)
		{
			sprintf(jstart+strlen(jstart), "\"cond_2\":[");
			for (i = 0; i < rt_set->gpi_trigger.cond_2.count; ++i)
				sprintf(jstart+strlen(jstart), "{\"id\":%d,\"state\":%d},", rt_set->gpi_trigger.cond_2.ids[i], 
					rt_set->gpi_trigger.cond_2.states[i]);

			sprintf(jstart+strlen(jstart)-1, "]");
		}

		sprintf(jstart+strlen(jstart), "},");
	}
	
	if (rt_set->gpo_act.count > 0)
	{
		sprintf(jstart+strlen(jstart), "\"gpo_act\":[");
		for (i = 0; i < rt_set->gpo_act.count; ++i)
		{
			sprintf(jstart+strlen(jstart), "{\"id\":%d,\"state\":%d,\"dur\":%d},", rt_set->gpo_act.ids[i], 
				rt_set->gpo_act.states[i], rt_set->gpo_act.durs[i]);	
		}
		sprintf(jstart+strlen(jstart)-1, "],");
	}
	
	if (rt_set->events.count > 0)
	{
		sprintf(jstart+strlen(jstart), "\"events\":[");
		for (i = 0; i < rt_set->events.count; ++i)
			sprintf(jstart+strlen(jstart), "%d,", rt_set->events.ids[i]);	

		sprintf(jstart+strlen(jstart)-1, "],");
	}
	
	
	if (rt_set->tag_json_format > 0)
	{
		sprintf(jstart+strlen(jstart), "\"tag_json_format\":[");
		for (i = 0; i < 16; ++i)
		{
			if (((rt_set->tag_json_format >> i) & 0x01) == 0x01)
				sprintf(jstart+strlen(jstart), "%d,", i);
		}
		sprintf(jstart+strlen(jstart)-1, "],");		
	}
	else
		sprintf(jstart+strlen(jstart), "\"tag_json_format\":[],");
	
	if (rt_set->cus_param.len > 0)
		sprintf(jstart+strlen(jstart), "\"cus_param\":\"%s\",", (char *)rt_set->cus_param.param);
	
	sprintf(jstart+strlen(jstart)-1, "}");

	*len = strlen(jstart);
//	TRACE("len:%d, tojson_rdr_runtime_settings:%s\n", *len, jstart);	
}

int valid_wlan_config_byobj(json_value *jvalue, WlanConfig_ST *wlanst);
int valid_bluetooth_config_byobj(json_value *jvalue, 
	BluetoothConfig_ST *blest);
int valid_monet_config_byobj(json_value *jvalue, 
	MonetConfig_ST *blest);
int valid_static_settings_byobj(json_value *jvalue, 
	ReaderStaticSettings_ST *static_set)
{
	int gen2_prof_tab[] = {-2, -1, 0, 1, 2, 3, 16, 17, 18, 19, 20, 21, 101, 103, 105, 107, 111, 112, 113, 115};
	int gen2_session_tab[] = {-1, 0, 1, 2, 3};
	int gen2_target_tab[] = {-1, 0, 1, 2, 3};
	int gen2_q_tab[] = {-2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
	int rf_region_tab[] = {-1, 0x01, 0x02, 0x07, 0x08, 0x03, 0x06, 0x0A, 0xFF, 0x04, 
								0x05, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 
								0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E};
	int rf_hop_mode_tab[] = {-1, 0x00, 0x01, 0x02};
	int bool_tab[] = {-1, 0, 1};
	int baud_tab[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
	int databit_tab[] = {0, 1};
	int stopbit_tab[] = {0, 1};
	int parity_tab[] = {0, 1, 2};
	int flow_ctrl_tab[] = {0, 1};
	int uarttype_tab[] = {0, 1};
//	int act_range_tab[] = {1, 2, 3};
	int reboot_tab[] = {0, 1};
	int i;
	int pvalint;

	json_value *obj;
	int exflag;

	PARSE_CHK_RANGE(jvalue, "reset", static_set->reset, 0, 1, 0);
	if (exflag == 0)
		static_set->reset = 0;
	
	PARSE_CHK_IN_TAB(jvalue, "reboot", static_set->reboot, reboot_tab, 0);
	
	if (json_getobject(jvalue, "app_init", &obj) == 0)
	{
		PARSE_CHK_RANGE(obj, "usb_type", static_set->app_init.usb_type, 1, 2, 0);
		PARSE_CHK_RANGE(obj, "max_tb_rec_len", static_set->app_init.max_tb_rec_len, 2, 190, 0);
		PARSE_CHK_RANGE(obj, "evt_que_len", static_set->app_init.evt_que_len, 30, 100, 0);
	}
	

	if (json_getobject(jvalue, "tagops_param.inventory", &obj) == 0)
	{
		json_value *tmpobj;
		
		PARSE_CHK_RANGE(obj, "cycle", static_set->tagops_param.inventory.cycle, 0, 0xffff, 0);

		PARSE_CHK_RANGE(obj, "interval", static_set->tagops_param.inventory.interval, 0, 0xffff, 0);

		PARSE_CHK_RANGE(obj, "inv_mode", static_set->tagops_param.inventory.inv_mode, -1, 20, 0);
		
		if (json_getobject(obj, "ants", &tmpobj) == 0)
		{
			if (tmpobj->type == json_array)
			{
				char tmpselbuf[8];
				int aid;
				int antcnt = tmpobj->u.array.length;
				if (antcnt > 16 || antcnt == 0)
				{
					TRACE("inv_ants obj length is invalid\n");
					return -1;
				}
					
				for (i = 0; i < antcnt; ++i)
				{
					sprintf(tmpselbuf, "[%d]", i);
					if (json_getint(tmpobj, tmpselbuf, &aid) != 0)
					{
						TRACE("inv_ants item %d dose not find\n", i);
						return -1;
					}
					if (aid < 1 && aid > gAntNumber)
					{
						TRACE("inv_ants item is invalid\n");
						return -1;
					}
					static_set->tagops_param.inventory.ants[i] = aid;
				}
				static_set->tagops_param.inventory.ants_cnt = antcnt;
				if (nonrep_int_array(static_set->tagops_param.inventory.ants, 
					antcnt) != 1)
				{
					TRACE("repeatable ant id\n");
					return -1;					
				}
			}
			else
			{
				TRACE("inventory.ants obj type is invalid\n");
				return -1;
			}
		}
	}
	
	if (json_getobject(jvalue, "tagops_param.tagfilter", &obj) == 0)
	{
		char *maskbuf;
		
		PARSE_CHK_RANGE(obj, "bank", static_set->tagops_param.tagfilter.bank, 1, 3, 1);
		
		PARSE_CHK_RANGE(obj, "start", static_set->tagops_param.tagfilter.start, 0, 65535, 1);
		
		PARSE_CHK_RANGE(obj, "match", static_set->tagops_param.tagfilter.match, 0, 1, 1);
		
		maskbuf = malloc_hexp(513);
		if (json_getstring_len(obj, "mask", 512, 2, maskbuf) != 0)
		{
			TRACE("mask dose not find\n");
			free_hexp(maskbuf);
			return -1;
		}
		
		Str2Binary(maskbuf, strlen(maskbuf), static_set->tagops_param.tagfilter.mask);
		static_set->tagops_param.tagfilter.mask_len = strlen(maskbuf);
		free_hexp(maskbuf);
		static_set->tagops_param.tagfilter.is_tagfilter = 1;
	}
	
	if (json_getobject(jvalue, "tagops_param.bankdata", &obj) == 0)
	{
		PARSE_CHK_RANGE(obj, "bank", static_set->tagops_param.bankdata.bank, 0, 3, 1);
		
		PARSE_CHK_RANGE(obj, "start", static_set->tagops_param.bankdata.start, 0, 255, 1);

		PARSE_CHK_RANGE(obj, "blkcnt", static_set->tagops_param.bankdata.blkcnt, 1, 16, 1);

		static_set->tagops_param.bankdata.is_bankdata = 1;
	}
	
	if (json_getobject(jvalue, "tagops_param.accessop", &obj) == 0)
	{
		char boppwd[10];
		
		if (gAntNumber == -1)
		{
			PARSE_CHK_RANGE(obj, "ant", static_set->tagops_param.accessop.ant, 0, 16, 0);
		}
		else
		{
			PARSE_CHK_RANGE(obj, "ant", static_set->tagops_param.accessop.ant, 0, gAntNumber, 0);
		}
		
		PARSE_CHK_RANGE(obj, "timeout", static_set->tagops_param.accessop.timeout, 50, 65535, 0);
	
		PARSE_CHK_STR(obj, "aespwd", boppwd, 8, 0, 0);
		if (exflag)
		{
			if (strlen(boppwd) != 8)
			{
				TRACE("aespwd len is invalid\n");
				return -1;				
			}
			if (strTohex(boppwd, 8, static_set->tagops_param.accessop.aespwd) != 0)
			{
				TRACE("aespwd value is invalid\n");
				return -1;				
			}
		}
	}
	
	if (json_getobject(jvalue, "tagops_param.mb_sinv_tag_fmt", &obj) == 0)
	{
		if (obj->type == json_array)
		{
			int tjfcnt = obj->u.array.length;
			char tmpselbuf[8];
			int tjfid;
			
			if (tjfcnt > 6)
			{
				TRACE("mb_sinv_tag_fmt obj length is invalid\n");
				return -1;
			}
			static_set->tagops_param.mb_sinv_tag_fmt = 0;
			for (i = 0; i < tjfcnt; ++i)
			{
				sprintf(tmpselbuf, "[%d]", i);
				if (json_getint(obj, tmpselbuf, &tjfid) != 0)
				{
					TRACE("mb_sinv_tag_fmt item is invalid\n");
					return -1;
				}
				static_set->tagops_param.mb_sinv_tag_fmt |= 1 << tjfid;
			}
		}
		else
		{
			TRACE("mb_sinv_tag_fmt obj type is invalid\n");
			return -1;
		}			
	}
	
	if (json_getobject(jvalue, "protocol.gen2", &obj) == 0)
	{
		PARSE_CHK_IN_TAB(obj, "session", static_set->protocol.gen2.session, gen2_session_tab, 0);
			
		PARSE_CHK_IN_TAB(obj, "q", static_set->protocol.gen2.q, gen2_q_tab, 0);

		PARSE_CHK_IN_TAB(obj, "profile", static_set->protocol.gen2.profile, gen2_prof_tab, 0);
			
		PARSE_CHK_IN_TAB(obj, "target", static_set->protocol.gen2.target, gen2_target_tab, 0);
	}
		
	if (json_getobject(jvalue, "rf", &obj) == 0)
	{
		json_value *pwrobj;
		json_value *htbobj;

		PARSE_CHK_IN_TAB(obj, "region", static_set->rf.region, rf_region_tab, 0);

		PARSE_CHK_IN_TAB(obj, "hop_mode", static_set->rf.hop_mode, rf_hop_mode_tab, 0);
			
		PARSE_CHK_RANGE(obj, "ant_max_dwell_time", static_set->rf.ant_max_dwell_time, 0, 0xffff, 0);
		
		if (json_getobject(obj, "tx_powers", &pwrobj) == 0)
		{
			int aid;
			
			if (pwrobj->type == json_array)
			{
				int pwrcnt = pwrobj->u.array.length;
				if (pwrcnt == 0 || pwrcnt > 16)
				{
					TRACE("tx_powers obj length is invalid\n");
					return -1;
				}
				/*
				for (i = 0; i < 16; ++i)
				{
					static_set->rf.tx_powers[i].read_power = -1;
					static_set->rf.tx_powers[i].write_power = -1;
				}
				*/
				for (i = 0; i < pwrcnt; ++i)
				{
					PARSE_CHK_RANGE(pwrobj->u.array.values[i], "id", aid, 1, 16, 1);
					
					if (json_getint(pwrobj->u.array.values[i], "rp", &pvalint) == 0)
					{
						if (pvalint < -2 || pvalint > 3300)
						{
							TRACE("rp is invalid\n");
							return -1;
						}
						static_set->rf.tx_powers[aid-1].read_power = pvalint;
					}
					else
					{
						TRACE("rp not find\n");
						return -1;						
					}
					
					if (json_getint(pwrobj->u.array.values[i], "wp", &pvalint) == 0)
					{
						if (pvalint < -2 || pvalint > 3300)
						{
							TRACE("wp is invalid\n");
							return -1;
						}
						static_set->rf.tx_powers[aid-1].write_power = pvalint;
					}
					else
					{
						TRACE("wp not find\n");
						return -1;						
					}
				}
			}
			else
			{
				TRACE("tx_powers obj type is invalid\n");
				return -1;
			}
		}
		
		if (json_getobject(obj, "hop_table", &htbobj) == 0)
		{
			if (htbobj->type == json_array)
			{
				int htblen = htbobj->u.array.length;
				char tmpselbuf[8];
				
				if (htblen > 50)
				{
					TRACE("hop_table obj length is invalid\n");
					return -1;
				}
				for (i = 0; i < htblen; ++i)
				{
					sprintf(tmpselbuf, "[%d]", i);
					if (json_getint(htbobj, tmpselbuf, &static_set->rf.hop_table[i]) != 0)
					{
						TRACE("hop_table item is invalid\n");
						return -1;
					}
				}
				static_set->rf.hop_table_cnt = htblen;
			}
			else
			{
				TRACE("hop_table obj type is invalid\n");
				return -1;
			}
		}
	}
	
	if (json_getobject(jvalue, "tag_data", &obj) == 0)
	{
		PARSE_CHK_IN_TAB(obj, "unique_by_antenna", static_set->tag_data.unique_by_antenna, bool_tab, 0);
			
		PARSE_CHK_IN_TAB(obj, "unique_by_bank_data", static_set->tag_data.unique_by_bank_data, bool_tab, 0);
			
		PARSE_CHK_IN_TAB(obj, "record_highest_rssi", static_set->tag_data.record_highest_rssi, bool_tab, 0);			
	}
	
	if (json_getobject(jvalue, "gpos", &obj) == 0)
	{
		if (obj->type == json_array)
		{
			int gpocnt = obj->u.array.length;
			int gid;
			if (gpocnt > 5)
			{
				TRACE("gpos obj length is invalid\n");
				return -1;
			}

			if (gpocnt == 0)
			{
				for (i = 0; i < 5; ++i)
					static_set->gpos[i] = 0;
			}

			for (i = 0; i < gpocnt; ++i)
			{
				PARSE_CHK_RANGE(obj->u.array.values[i], "id", gid, 1, 5, 1);

				PARSE_CHK_RANGE(obj->u.array.values[i], "state", pvalint, 0, 1, 1);
				static_set->gpos[gid-1] = pvalint+1;
			}
		}
		else
		{
			TRACE("gpos obj type is invalid\n");
			return -1;
		}
	}
	

	if (json_getobject(jvalue, "uart1", &obj) == 0)
	{
		PARSE_CHK_IN_TAB(obj, "baud", static_set->uart1.baud, baud_tab, 0);
			
		PARSE_CHK_RANGE(obj, "address", static_set->uart1.address, 1, 254, 0);

		PARSE_CHK_IN_TAB(obj, "stop_bits", static_set->uart1.stop_bits, stopbit_tab, 0);
			
		PARSE_CHK_IN_TAB(obj, "data_bits", static_set->uart1.data_bits, databit_tab, 0);
			
		PARSE_CHK_IN_TAB(obj, "parity", static_set->uart1.parity, parity_tab, 0);
			
		PARSE_CHK_IN_TAB(obj, "flow_ctrl", static_set->uart1.flow_ctrl, flow_ctrl_tab, 0);
	}
	
	if (json_getobject(jvalue, "uart2", &obj) == 0)
	{
		PARSE_CHK_IN_TAB(obj, "baud", static_set->uart2.baud, baud_tab, 0);
			
		PARSE_CHK_RANGE(obj, "address", static_set->uart2.address, 1, 254, 0);
		
		PARSE_CHK_IN_TAB(obj, "type", static_set->uart2.type, uarttype_tab, 0);

		PARSE_CHK_IN_TAB(obj, "stop_bits", static_set->uart2.stop_bits, stopbit_tab, 0);
			
		PARSE_CHK_IN_TAB(obj, "data_bits", static_set->uart2.data_bits, databit_tab, 0);
			
		PARSE_CHK_IN_TAB(obj, "parity", static_set->uart2.parity, parity_tab, 0);
			
		PARSE_CHK_IN_TAB(obj, "flow_ctrl", static_set->uart2.flow_ctrl, flow_ctrl_tab, 0);
	}
	
	static_set->is_ethernet = 0;
	if (json_getobject(jvalue, "ethernet", &obj) == 0)
	{
		if (get_spi_ex_dev() == Spi_Ex_Ethernet)
		{
			memcpy(&static_set->ethernet, &gNetConf, sizeof(gNetConf));
			if (valid_ipinfo_cfg(obj, &static_set->ethernet) != 0)
			{
				TRACE("ethernet is invalid\n");
				return -1;		
			}
			static_set->is_ethernet = 1;
		}
		else
		{
			TRACE("no ethernet interface\n");
			return -1;	
		}
	}
	
	static_set->uart_ex_type = Uart_Ex_None;
	if (json_getobject(jvalue, "wlan", &obj) == 0)
	{
		if (get_uart_ex_dev() == Uart_Ex_Wlan)
		{
			setdef_wlan_config(&static_set->uart_ex.wlan);
			if (valid_wlan_config_byobj(obj, &static_set->uart_ex.wlan) != 0)
			{
				TRACE("wlan is invalid\n");
				return -1;		
			}
			static_set->uart_ex_type = Uart_Ex_Wlan;
		}
		else
		{
			TRACE("no wlan interface\n");
			return -1;	
		}
	}	
	
	if (json_getobject(jvalue, "bluetooth", &obj) == 0)
	{
		if (get_uart_ex_dev() == Uart_Ex_Bluetooth)
		{
			setdef_bluetooth_config(&static_set->uart_ex.ble);
			if (valid_bluetooth_config_byobj(obj, &static_set->uart_ex.ble) != 0)
			{
				TRACE("bluetooth is invalid\n");
				return -1;		
			}
			static_set->uart_ex_type = Uart_Ex_Bluetooth;
		}
		else
		{
			TRACE("no bluetooth interface\n");
			return -1;	
		}
	}
	
	if (json_getobject(jvalue, "monet", &obj) == 0)
	{
		if (get_uart_ex_dev() == Uart_Ex_4G)
		{
			setdef_monet_config(&static_set->uart_ex.monet);
			if (valid_monet_config_byobj(obj, &static_set->uart_ex.monet) != 0)
			{
				TRACE("monet is invalid\n");
				return -1;		
			}
			static_set->uart_ex_type = Uart_Ex_4G;
		}
		else
		{
			TRACE("no monet interface\n");
			return -1;	
		}
	}
	
	return 0;
}

int valid_static_settings(char *buf, int blen, ReaderStaticSettings_ST *static_set)
{
	json_value *jvalue;
	
	jvalue = json_parse(buf, blen);
	if (jvalue == NULL)
	{
		TRACE("valid_static_settings jvalue == NULL\n");
		return -1;
	}
	return valid_static_settings_byobj(jvalue, static_set);
}

int cmd_config_static_settings(json_value *jvalue, char *cfg_name, 
	char *buf, int blen, int *reboot)
{
	ReaderStaticSettings_ST *rssettings;
	int validret;
	
	rssettings = malloc_hexp(sizeof(ReaderStaticSettings_ST));
	if (jvalue == NULL)
		validret = valid_static_settings(buf, blen, rssettings);
	else
		validret = valid_static_settings_byobj(jvalue, rssettings);
	
	if (validret == 0)
	{
		setdef_rdr_static_settings(rssettings);
		if (rssettings->reset == 0)
			get_rdr_static_settings(rssettings);

		if (jvalue == NULL)
			validret = valid_static_settings(buf, blen, rssettings);
		else
		{
			json_value *obj1;
			json_value *obj2;
			obj1 = json_parse(buf, blen);
			if (obj1 != NULL)
			{
				if (json_getobject(obj1, cfg_name, &obj2) == 0)
					validret = valid_static_settings_byobj(obj2, rssettings);
				else
				{
					TRACE("valid_static_settings_byobj(obj2, rssettings) failed\n");
					validret = -1;
				}
			}
			else
			{
				TRACE("cmd_config_static_settings obj1 == NULL\n");
				validret = -1;		
			}
		}
		
		if (validret == 0)
		{
			dump_static_settings(rssettings);
			if (rssettings->is_ethernet == 1)
			{
				TRACE("if (rssettings->is_ethernet == 1)\n");
				set_network_config(&rssettings->ethernet);
				sleep_ms(150);
			}
			if (rssettings->uart_ex_type == Uart_Ex_Wlan)
			{
				TRACE("if (rssettings->uart_ex_type == Uart_Ex_Wlan)\n");
				set_wlan_config(&rssettings->uart_ex.wlan);
				sleep_ms(150);
			}
			if (rssettings->uart_ex_type == Uart_Ex_Bluetooth)
			{
				TRACE("if (rssettings->uart_ex_type == Uart_Ex_Bluetooth)\n");
				set_bluetooth_config(&rssettings->uart_ex.ble);
				sleep_ms(150);
			}
			if (rssettings->uart_ex_type == Uart_Ex_4G)
			{
				TRACE("if (rssettings->uart_ex_type == Uart_Ex_4G)\n");
				set_monet_config(&rssettings->uart_ex.monet);
				sleep_ms(150);
			}
			set_rdr_static_settings(rssettings);
			*reboot = rssettings->reboot;
		}
	}
	
	free_hexp(rssettings);
	return validret;
}
/*
int valid_modbus_uart(char *buf, int blen, rdr_st_set_modbus_uart *moduart)
{
	int baud_tab[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
	int databit_tab[] = {0, 1};
	int stopbit_tab[] = {0, 1};
	int parity_tab[] = {0, 1, 2};
	int flow_ctrl_tab[] = {0, 1};
	int uarttype_tab[] = {0, 1};
//	int reset = 1;
	int pvalint;

	json_value *obj;
	
	moduart->is_modbus_uart = 0;
	
	obj = json_parse(buf, blen);
	if (obj == NULL)
	{
		TRACE("valid_modbus_uart jvalue == NULL\n");
		return -1;
	}

	PARSE_CHK_IN_TAB(obj, "baud", moduart->baud,  baud_tab, 1);
			
	PARSE_CHK_RANGE(obj, "address", moduart->address, 2, 255, 1);
		
	PARSE_CHK_IN_TAB(obj, "type", moduart->type, uarttype_tab, 1);

	PARSE_CHK_IN_TAB(obj, "stop_bits", moduart->stop_bits, stopbit_tab, 1);
			
	PARSE_CHK_IN_TAB(obj, "data_bits", moduart->data_bits, databit_tab, 1);
			
	PARSE_CHK_IN_TAB(obj, "parity", moduart->parity, parity_tab, 1);
			
	PARSE_CHK_IN_TAB(obj, "flow_ctrl", moduart->flow_ctrl, flow_ctrl_tab, 1);
	
	moduart->is_modbus_uart = 1;
	return 0;
}
*/
extern uint8 gMonetState;
void tojson_rdr_static_settings(ReaderStaticSettings_ST *static_set, 
	char *jstart, int *len, int isexcfg)
{
	int i;
	int isgpo = 0;
	
	sprintf(jstart, "{\"app_init\":{\"usb_type\":%d,\"max_tb_rec_len\":%d,\"evt_que_len\":%d},", 
		static_set->app_init.usb_type, static_set->app_init.max_tb_rec_len, static_set->app_init.evt_que_len);

	sprintf(jstart+strlen(jstart), "\"tagops_param\":{\"inventory\":{\"cycle\":%d,\"interval\":%d,\"inv_mode\":%d,",
		static_set->tagops_param.inventory.cycle, static_set->tagops_param.inventory.interval, 
		static_set->tagops_param.inventory.inv_mode);
	
	if (static_set->tagops_param.inventory.ants_cnt > 0)
	{
		sprintf(jstart+strlen(jstart), "\"ants\":[");
		for (i = 0; i < static_set->tagops_param.inventory.ants_cnt; ++i)
			sprintf(jstart+strlen(jstart), "%d,",static_set->tagops_param.inventory.ants[i]);

		sprintf(jstart+strlen(jstart)-1, "],");
	}
	sprintf(jstart+strlen(jstart)-1, "},");

	if (static_set->tagops_param.tagfilter.is_tagfilter == 1)
	{
		char *binstr;

		sprintf(jstart+strlen(jstart), "\"tagfilter\":{\"bank\":%d,\"start\":%d,\"match\":%d,\"mask\":\"", 
			static_set->tagops_param.tagfilter.bank, static_set->tagops_param.tagfilter.start, 
			static_set->tagops_param.tagfilter.match);
		
		binstr = malloc_hexp(513);
		Data2BinStr(static_set->tagops_param.tagfilter.mask, 
			static_set->tagops_param.tagfilter.mask_len, binstr);
		sprintf(jstart+strlen(jstart), "%s\"},", binstr);
		free_hexp(binstr);
	}
	
	if (static_set->tagops_param.bankdata.is_bankdata == 1)
		sprintf(jstart+strlen(jstart), "\"bankdata\":{\"bank\":%d,\"start\":%d,\"blkcnt\":%d},", 
			static_set->tagops_param.bankdata.bank, static_set->tagops_param.bankdata.start, 
			static_set->tagops_param.bankdata.blkcnt);

	sprintf(jstart+strlen(jstart), "\"accessop\":{\"ant\":%d,\"timeout\":%d,", 
		static_set->tagops_param.accessop.ant, static_set->tagops_param.accessop.timeout);
	if (GetNumU32(static_set->tagops_param.accessop.aespwd) != 0)
		sprintf(jstart+strlen(jstart), "\"aespwd\":\"%08X\"},", 
			GetNumU32(static_set->tagops_param.accessop.aespwd));
	else
		sprintf(jstart+strlen(jstart)-1, "},");
	
	if (static_set->tagops_param.mb_sinv_tag_fmt > 0)
	{
		sprintf(jstart+strlen(jstart), "\"mb_sinv_tag_fmt\":[");
		for (i = 0; i < 16; ++i)
		{
			if (((static_set->tagops_param.mb_sinv_tag_fmt >> i) & 0x01) == 0x01)
				sprintf(jstart+strlen(jstart), "%d,", i);
		}
		sprintf(jstart+strlen(jstart)-1, "],");		
	}
	else
		sprintf(jstart+strlen(jstart), "\"mb_sinv_tag_fmt\":[],");
	
	sprintf(jstart+strlen(jstart)-1, "},");
	
	sprintf(jstart+strlen(jstart), "\"protocol\":{\"gen2\":{\"session\":%d,\"q\":%d,\"target\":%d,\"profile\":%d}},", 
		static_set->protocol.gen2.session, static_set->protocol.gen2.q, static_set->protocol.gen2.target, 
		static_set->protocol.gen2.profile);

	sprintf(jstart+strlen(jstart), "\"rf\":{\"ant_max_dwell_time\":%d,\"hop_mode\":%d,\"region\":%d,", 
		static_set->rf.ant_max_dwell_time, static_set->rf.hop_mode, static_set->rf.region);	

	if (static_set->rf.hop_table_cnt != 0)
	{
		sprintf(jstart+strlen(jstart), "\"hop_table\":[");
		for (i = 0; i < static_set->rf.hop_table_cnt; ++i)
			sprintf(jstart+strlen(jstart), "%d,", static_set->rf.hop_table[i]);

		sprintf(jstart+strlen(jstart)-1, "],");	
	}
		

	sprintf(jstart+strlen(jstart), "\"tx_powers\":[");
	if (gAntNumber == -1)
		sprintf(jstart+strlen(jstart), "],");
	else
	{
		for (i = 0; i < gAntNumber; ++i)
			sprintf(jstart+strlen(jstart), "{\"id\":%d,\"rp\":%d,\"wp\":%d},", i+1, static_set->rf.tx_powers[i].read_power, 
				static_set->rf.tx_powers[i].write_power);
		sprintf(jstart+strlen(jstart)-1, "],");
	}
	sprintf(jstart+strlen(jstart)-1, "},");
		

	sprintf(jstart+strlen(jstart), "\"tag_data\":{\"unique_by_antenna\":%d,\"unique_by_bank_data\":%d,\"record_highest_rssi\":%d},", 
		static_set->tag_data.unique_by_antenna, static_set->tag_data.unique_by_bank_data, 
		static_set->tag_data.record_highest_rssi);		

	for (i = 0; i < 4; ++i)
	{
		if (static_set->gpos[i] != 0)
		{
			isgpo = 1;
			break;
		}
	}
	if (isgpo == 1)
	{
		sprintf(jstart+strlen(jstart), "\"gpos\":[");
		for (i = 0; i < 4; ++i)
		{
			if (static_set->gpos[i] != 0)
				sprintf(jstart+strlen(jstart), "{\"id\":%d,\"state\":%d},", i+1, 
					static_set->gpos[i]-1);
		}
		sprintf(jstart+strlen(jstart)-1, "],");
	}

	sprintf(jstart+strlen(jstart), "\"uart1\":{");
	sprintf(jstart+strlen(jstart), "\"type\":%d,", static_set->uart1.type);
	sprintf(jstart+strlen(jstart), "\"address\":%d,", static_set->uart1.address);
	sprintf(jstart+strlen(jstart), "\"baud\":%d,", static_set->uart1.baud);
	sprintf(jstart+strlen(jstart), "\"data_bits\":%d,", static_set->uart1.data_bits);
	sprintf(jstart+strlen(jstart), "\"stop_bits\":%d,", static_set->uart1.stop_bits);
	sprintf(jstart+strlen(jstart), "\"flow_ctrl\":%d,", static_set->uart1.flow_ctrl);
	sprintf(jstart+strlen(jstart), "\"parity\":%d},", static_set->uart1.parity);	

	sprintf(jstart+strlen(jstart), "\"uart2\":{");
	sprintf(jstart+strlen(jstart), "\"type\":%d,", static_set->uart2.type);
	sprintf(jstart+strlen(jstart), "\"address\":%d,", static_set->uart2.address);
	sprintf(jstart+strlen(jstart), "\"baud\":%d,", static_set->uart2.baud);
	sprintf(jstart+strlen(jstart), "\"data_bits\":%d,", static_set->uart2.data_bits);
	sprintf(jstart+strlen(jstart), "\"stop_bits\":%d,", static_set->uart2.stop_bits);
	sprintf(jstart+strlen(jstart), "\"flow_ctrl\":%d,", static_set->uart2.flow_ctrl);
	sprintf(jstart+strlen(jstart), "\"parity\":%d},", static_set->uart2.parity);	
	
	if (get_spi_ex_dev() == Spi_Ex_Ethernet  && isexcfg == 1)
	{
		sprintf(jstart+strlen(jstart), "\"ethernet\":{");
		sprintf(jstart+strlen(jstart), "\"ip\":\"%d.%d.%d.%d\",", gNetConf.ip[0], 
			gNetConf.ip[1], gNetConf.ip[2], gNetConf.ip[3]);
		sprintf(jstart+strlen(jstart), "\"nm\":\"%d.%d.%d.%d\",", gNetConf.subnetMask[0], 
			gNetConf.subnetMask[1], gNetConf.subnetMask[2], gNetConf.subnetMask[3]);
		sprintf(jstart+strlen(jstart), "\"gw\":\"%d.%d.%d.%d\",", gNetConf.gatewayIP[0], 
			gNetConf.gatewayIP[1], gNetConf.gatewayIP[2], gNetConf.gatewayIP[3]);
		sprintf(jstart+strlen(jstart), "\"dns\":\"%d.%d.%d.%d\",", gNetConf.dnsServer[0], 
			gNetConf.dnsServer[1], gNetConf.dnsServer[2], gNetConf.dnsServer[3]);
		sprintf(jstart+strlen(jstart), "\"mac\":\"%02X%02X%02X%02X%02X%02X\",", gNetConf.mac[0], 
			gNetConf.mac[1], gNetConf.mac[2], gNetConf.mac[3], gNetConf.mac[4], gNetConf.mac[5]);
		sprintf(jstart+strlen(jstart), "\"lport\":%d},", gNetConf.listenPort);
	}
	
	if (get_uart_ex_dev() == Uart_Ex_Wlan && isexcfg == 1)
	{
		WlanConfig_ST *wlanconf = malloc_hexp(sizeof(WlanConfig_ST));
		int wclen;
		get_wlan_config(wlanconf);
		sprintf(jstart+strlen(jstart), "\"wlan\":");
		tojson_wlan_config(wlanconf, jstart+strlen(jstart), &wclen);
		sprintf(jstart+strlen(jstart), ",");
		free_hexp(wlanconf);
	}
	
	if (get_uart_ex_dev() == Uart_Ex_Bluetooth && isexcfg == 1)
	{
		BluetoothConfig_ST *blest = malloc_hexp(sizeof(BluetoothConfig_ST));
		int btlen;
		get_bluetooth_config(blest);
		
		sprintf(jstart+strlen(jstart), "\"bluetooth\":");
		tojson_bluetooth_config(blest, jstart+strlen(jstart), &btlen);
		sprintf(jstart+strlen(jstart), ",");
		free_hexp(blest);
	}
	
	if (get_uart_ex_dev() == Uart_Ex_4G && isexcfg == 1)
	{
		MonetConfig_ST *monst = malloc_hexp(sizeof(MonetConfig_ST));
		int molen;
		get_monet_config(monst);
		
		sprintf(jstart+strlen(jstart), "\"monet\":");
		tojson_monet_config(monst, jstart+strlen(jstart), &molen);
		sprintf(jstart+strlen(jstart), ",");
		free_hexp(monst);
	}		
	sprintf(jstart+strlen(jstart)-1, "}");
	
	*len = strlen(jstart);
//	TRACE("tojson_rdr_static_settings:%s\n", jstart);
}

int set_rdr_runtime_settings(ReaderRunTimeSettings_ST *rt_set)
{
	int err = 0;
	ConfReadBuffer confBuf;
	int pos = strlen(ActModeCfg_Marker);
	char *jstart;
	int jlen;
	E(pre_set_config(&confBuf));
	memcpy(confBuf.activemodecfgbuf, ActModeCfg_Marker, pos);
//	SetNumU16(confBuf.passivemodecfgbuf+pos, len);
	jstart = (char *)confBuf.activemodecfgbuf+pos+2;
	
	tojson_rdr_runtime_settings(rt_set, jstart, &jlen);
    deviceID_update(rt_set);
	set_eastag_to_flash();
	SetNumU16(confBuf.activemodecfgbuf+pos, jlen);
	E(set_config_to_flash(&confBuf));
FIN:
	return err;	
}

int set_rdr_static_settings(ReaderStaticSettings_ST *static_set)
{
	int err = 0;
	ConfReadBuffer confBuf;
	int pos = strlen(RdrStaticSets_Marker);
	char *jstart;
	int jlen;
	E(pre_set_config(&confBuf));
	memcpy(confBuf.passivemodecfgbuf, RdrStaticSets_Marker, pos);
//	SetNumU16(confBuf.passivemodecfgbuf+pos, len);
	jstart = (char *)confBuf.passivemodecfgbuf+pos+2;
	
	tojson_rdr_static_settings(static_set, jstart, &jlen, 0);

	SetNumU16(confBuf.passivemodecfgbuf+pos, jlen);
	E(set_config_to_flash(&confBuf));
FIN:
	return err;	
}

int get_rdr_runtime_settings(ReaderRunTimeSettings_ST *rt_set)
{
	int pos = strlen(ActModeCfg_Marker);
	char *rtbuf = malloc_hexp(MaxActiveModeConfigLen);
	
	if (rtbuf == NULL)
	{
		TRACE("get_rdr_runtime_settings:no enough memory\n");
		return -1;
	}
	
	setdef_rdr_runtime_settings(rt_set);
	flash_bytes_read(ActiveModeConfig_Addr, rtbuf, MaxActiveModeConfigLen);
	if (memcmp(ActModeCfg_Marker, rtbuf, pos) == 0)
	{
		int jsonlen = GetNumU16((uint8 *)rtbuf+pos);
		int vlret;
		pos += 2;
		vlret = valid_runtime_settings(rtbuf+pos, jsonlen, rt_set);
		TRACE("get_rdr_runtime_settings valid_runtime_settings vlret:%d\n", vlret);
		if (vlret != 0)
			{setdef_rdr_runtime_settings(rt_set);}
	    else
		  { deviceID_update(rt_set);}		
		free_hexp(rtbuf);
		
		return vlret;
	}
	else
	{
		setdef_rdr_runtime_settings(rt_set);
		TRACE("not find ActModeCfg_Marker\n");
		free_hexp(rtbuf);
		return -1;
	}
}

int get_rdr_static_settings(ReaderStaticSettings_ST *static_set)
{
	int pos = strlen(RdrStaticSets_Marker);
	char *passbuf = malloc_hexp(MaxPassiveModeConfigLen);
	
	if (passbuf == NULL)
	{
		TRACE("get_rdr_static_settings:no enough memory\n");
		return -1;
	}
	setdef_rdr_static_settings(static_set);
	
	flash_bytes_read(PassiveModeConfig_Addr, passbuf, MaxPassiveModeConfigLen);
	if (memcmp(RdrStaticSets_Marker, passbuf, pos) == 0)
	{
		int jsonlen = GetNumU16((uint8 *)passbuf+pos);
		int vlret;
		pos += 2;
		vlret = valid_static_settings(passbuf+pos, jsonlen, static_set);
		if (vlret != 0)
			setdef_rdr_static_settings(static_set);
		TRACE("get_rdr_static_settings valid_static_settings vlret:%d\n", vlret);
		free_hexp(passbuf);
		return vlret;
	}
	else
	{
		setdef_rdr_static_settings(static_set);
		TRACE("not find RdrStaticSets_Marker\n");
		free_hexp(passbuf);
		return -1;
	}
}

int get_workmode_params(WorkMode_Code *wmode)
{
	uint8 header[32];
	int pos = strlen(WorkModeParams_Marker);
	
	flash_bytes_read(WorkModeParams_Addr, header, 32);
	if (memcmp(header, WorkModeParams_Marker, strlen(WorkModeParams_Marker)) != 0)
		*wmode = WorkMode_Passive;
	else
		*wmode = (WorkMode_Code)GetNumU32(header+pos);
	return 0;
}

int set_workmode_params(WorkMode_Code wmode)
{
	int err = 0;
	ConfReadBuffer confBuf;
	int pos = strlen(WorkModeParams_Marker);
	
	E(pre_set_config(&confBuf));
	memcpy(confBuf.workmodeparambuf, WorkModeParams_Marker, pos);
	SetNumU32(confBuf.workmodeparambuf+pos, wmode);
	E(set_config_to_flash(&confBuf));
	
FIN:
	return err;	
}

void AppCustomParams_To_runtime_settings(AppCustomParams *pAppCusPara, 
	ReaderRunTimeSettings_ST *runtime_set)
{
	int i;
	runtime_set->glob_params.hb_cylce = pAppCusPara->heart_beat_cylce;
	memcpy(runtime_set->glob_params.name, pAppCusPara->name, pAppCusPara->name_len);
	runtime_set->glob_params.name[pAppCusPara->name_len] = 0;
	runtime_set->glob_params.s_buf_size = 512;
	/*
	for (i = 0; i < 16; ++i)
		runtime_set->inventory.ants[i] = 0;
	for (i = 0; i < pAppCusPara->invants_len; ++i)
		runtime_set->inventory.ants[pAppCusPara->inv_ants[i]-1] = 1;

	runtime_set->inventory.cycle = pAppCusPara->inv_cycle;
	runtime_set->inventory.interval = pAppCusPara->interval_cycle;
	runtime_set->inventory.inv_mode = 0;
	
	runtime_set->tagfilter.is_tagfilter = pAppCusPara->is_tagfilter;
	if (pAppCusPara->is_tagfilter == 1)
	{
		runtime_set->tagfilter.bank = pAppCusPara->tag_filter.bank;
		runtime_set->tagfilter.mask_len = pAppCusPara->tag_filter.mask_len;
		runtime_set->tagfilter.match = pAppCusPara->tag_filter.is_match;
		memcpy(runtime_set->tagfilter.mask, pAppCusPara->tag_filter.mask, 64);
		runtime_set->tagfilter.start = pAppCusPara->tag_filter.start_bit;
	}
	
	runtime_set->bankdata.is_bankdata = pAppCusPara->is_inv_bank_read;
	if ( pAppCusPara->is_inv_bank_read == 1)
	{
		runtime_set->bankdata.bank = pAppCusPara->inv_bank_read.bank;
		runtime_set->bankdata.blkcnt = pAppCusPara->inv_bank_read.blockcnt;
		runtime_set->bankdata.start = pAppCusPara->inv_bank_read.start_block;
		memcpy(runtime_set->bankdata.pwd, pAppCusPara->inv_bank_read.pwd, 4);
	}
	*/
	runtime_set->cus_param.len = pAppCusPara->cusparam_len;
	if (pAppCusPara->cusparam_len > 0)
	{
		memcpy(runtime_set->cus_param.param, pAppCusPara->cusparam, 65);
		runtime_set->cus_param.param[pAppCusPara->cusparam_len] = 0;
	}
	runtime_set->gpo_act.count = pAppCusPara->loc_gpo_act_count;
	for (i = 0; i < pAppCusPara->loc_gpo_act_count; ++i)
	{
		runtime_set->gpo_act.ids[i] = pAppCusPara->lga_gpo_id[i];
		runtime_set->gpo_act.states[i] = pAppCusPara->lga_gpo_states[i];
		runtime_set->gpo_act.durs[i] = pAppCusPara->lga_gpo_durs[i];
	}

	runtime_set->upload.sw_potl = Upload_Trans_Potl_Tcp;
	if (pAppCusPara->upload_ip[0] == 0)
	{
		runtime_set->upload.hw_inf = Upload_Inf_Uart_1;
		/*
		runtime_set->upload.inf_params.uart.baud = (pAppCusPara->upload_ip[1] << 16) | 
			(pAppCusPara->upload_ip[2] << 8) | pAppCusPara->upload_ip[3];
		runtime_set->upload.inf_params.uart.data_bits = 0;
		runtime_set->upload.inf_params.uart.stop_bits = 0;
		runtime_set->upload.inf_params.uart.parity = 0;
		runtime_set->upload.inf_params.uart.flow_ctrl = 0;
		*/
		runtime_set->upload.crc_enable = 1;
	}
	else
	{
		runtime_set->upload.hw_inf = Upload_Inf_Ethernet;
		sprintf(runtime_set->upload.sw_potl_params.tcp.ser_ip, "%d.%d.%d.%d", 
			pAppCusPara->upload_ip[0], pAppCusPara->upload_ip[1], 
			pAppCusPara->upload_ip[2], pAppCusPara->upload_ip[3]);
		runtime_set->upload.sw_potl_params.tcp.ser_port = pAppCusPara->upload_port;
		runtime_set->upload.crc_enable = 0;
	}
	runtime_set->upload.client_ack = pAppCusPara->ack_client_mode;
	runtime_set->upload.data_aggr.mode = pAppCusPara->data_aggr_mode;
	runtime_set->upload.data_aggr.timeval = pAppCusPara->data_aggr_duration;

	if (pAppCusPara->event_count > 0)
		runtime_set->events.ids[runtime_set->events.count++] = rdr_rt_evt_GpiChange;
	
	runtime_set->gpi_trigger.is_gpi_trigger = pAppCusPara->is_gpi_trigger;
	if (pAppCusPara->is_gpi_trigger == 1)
	{
		runtime_set->gpi_trigger.mode = pAppCusPara->gpi_trigger_mode;
		runtime_set->gpi_trigger.timeval = pAppCusPara->gpi_read_timeout;
		runtime_set->gpi_trigger.cond_1.count = pAppCusPara->gpi_trigger1.gpi_count;
		for (i = 0;i < pAppCusPara->gpi_trigger1.gpi_count; ++i)
		{
			runtime_set->gpi_trigger.cond_1.ids[i] = pAppCusPara->gpi_trigger1.gpi_ids[i];
			runtime_set->gpi_trigger.cond_1.states[i] = pAppCusPara->gpi_trigger1.gpi_states[i];
		}
		
		if (pAppCusPara->gpi_trigger_mode == 1 || pAppCusPara->gpi_trigger_mode == 3
			|| pAppCusPara->gpi_trigger_mode == 4)
		{
			runtime_set->gpi_trigger.cond_2.count = pAppCusPara->gpi_trigger2.gpi_count;
			for (i = 0;i < pAppCusPara->gpi_trigger2.gpi_count; ++i)
			{
				runtime_set->gpi_trigger.cond_2.ids[i] = pAppCusPara->gpi_trigger2.gpi_ids[i];
				runtime_set->gpi_trigger.cond_2.states[i] = pAppCusPara->gpi_trigger2.gpi_states[i];
			}			
		}
	}
}

void dump_bluetooth_config(BluetoothConfig_ST *blest)
{
#ifdef _DEBUG
	TRACE("dump_bluetooth_config start\n");
	TRACE("name:%s", blest->name);
	TRACE("pwd_pair:%d", blest->pwd_pair);
	TRACE("std_ble_pair:%d", blest->std_ble_pair);
	TRACE("pwd:%s", blest->pwd);
	TRACE("dump_bluetoothn_config end\n");
#endif
}

void tojson_monet_config(MonetConfig_ST *monst, 
	char *jstart, int *len)
{
	sprintf(jstart, "{\"state\":%d,\"apn\":{\"enable\":%d,\"auth\":%d,\"cid\":%d,\"name\":\"%s\",\"user\":\"%s\",\"pwd\":\"%s\"}}", 
		gMonetState, monst->apn.enable, monst->apn.auth, monst->apn.cid, monst->apn.name, monst->apn.user, monst->apn.pwd);
}

void tojson_bluetooth_config(BluetoothConfig_ST *blest, 
	char *jstart, int *len)
{
	sprintf(jstart, "{\"name\":\"%s\",\"std_ble_pair\":%d,\"pwd_pair\":%d,", blest->name, 
		blest->std_ble_pair, blest->pwd_pair);	
	sprintf(jstart+strlen(jstart), "\"mac\":\"%02X%02X%02X%02X%02X%02X\",", blest->mac[0], 
		blest->mac[1], blest->mac[2], blest->mac[3], blest->mac[4], blest->mac[5]);
	
	if (blest->pwd_pair == 1)
		sprintf(jstart+strlen(jstart), "\"pwd\":\"%s\"}", blest->pwd);
	else
		sprintf(jstart+strlen(jstart)-1, "}");
}

void tojson_wlan_config(WlanConfig_ST *wlanst, char *jstart, int *len)
{
	sprintf(jstart, "{\"ip\":\"%d.%d.%d.%d\",", wlanst->ipinfo.ip[0], 
		wlanst->ipinfo.ip[1], wlanst->ipinfo.ip[2], wlanst->ipinfo.ip[3]);
	sprintf(jstart+strlen(jstart), "\"nm\":\"%d.%d.%d.%d\",", wlanst->ipinfo.subnetMask[0], 
		wlanst->ipinfo.subnetMask[1], wlanst->ipinfo.subnetMask[2], wlanst->ipinfo.subnetMask[3]);
	sprintf(jstart+strlen(jstart), "\"gw\":\"%d.%d.%d.%d\",", wlanst->ipinfo.gatewayIP[0], 
		wlanst->ipinfo.gatewayIP[1], wlanst->ipinfo.gatewayIP[2], wlanst->ipinfo.gatewayIP[3]);
	sprintf(jstart+strlen(jstart), "\"dns\":\"%d.%d.%d.%d\",", wlanst->ipinfo.dnsServer[0], 
		wlanst->ipinfo.dnsServer[1], wlanst->ipinfo.dnsServer[2], wlanst->ipinfo.dnsServer[3]);
	sprintf(jstart+strlen(jstart), "\"mac\":\"%02X%02X%02X%02X%02X%02X\",", wlanst->ipinfo.mac[0], 
		wlanst->ipinfo.mac[1], wlanst->ipinfo.mac[2], wlanst->ipinfo.mac[3], wlanst->ipinfo.mac[4], 
		wlanst->ipinfo.mac[5]);
	sprintf(jstart+strlen(jstart), "\"lport\":%d,", wlanst->ipinfo.listenPort);
	sprintf(jstart+strlen(jstart), "\"ssid\":\"%s\",\"mode\":%d,\"pwd\":\"%s\"}", 
		wlanst->ssid, wlanst->mode, wlanst->pwd);
	
	*len = strlen(jstart);
}

int erase_monet_config(void)
{
	int err = 0;
	ConfReadBuffer confBuf;
	E(pre_set_config(&confBuf));
	memset(confBuf.monetcfgbuf, 0, 32);
	E(set_config_to_flash(&confBuf));
FIN:
	return err;
}

int set_monet_config(MonetConfig_ST *monst)
{
	int err = 0;
	ConfReadBuffer confBuf;
	int pos = strlen(MonetConfig_Marker);
	char *jstart;
	int jlen;
	E(pre_set_config(&confBuf));
	memcpy(confBuf.monetcfgbuf, MonetConfig_Marker, pos);
	jstart = (char *)confBuf.monetcfgbuf+pos+2;
	
	tojson_monet_config(monst, jstart, &jlen);

	SetNumU16(confBuf.monetcfgbuf+pos, jlen);
	E(set_config_to_flash(&confBuf));
FIN:
	return err;
}

int set_bluetooth_config(BluetoothConfig_ST *blest)
{
	int err = 0;
	ConfReadBuffer confBuf;
	int pos = strlen(BluetoothConfig_Marker);
	char *jstart;
	int jlen;
	E(pre_set_config(&confBuf));
	memcpy(confBuf.bluetoothcfgbuf, BluetoothConfig_Marker, pos);
	jstart = (char *)confBuf.bluetoothcfgbuf+pos+2;
	
	tojson_bluetooth_config(blest, jstart, &jlen);

	SetNumU16(confBuf.bluetoothcfgbuf+pos, jlen);
	E(set_config_to_flash(&confBuf));
FIN:
	return err;	
}

int set_wlan_config(WlanConfig_ST *wlanst)
{
	int err = 0;
	ConfReadBuffer confBuf;
	int pos = strlen(WifiConfig_Marker);
	char *jstart;
	int jlen;
	E(pre_set_config(&confBuf));
	memcpy(confBuf.wificfgbuf, WifiConfig_Marker, pos);
	jstart = (char *)confBuf.wificfgbuf+pos+2;
	
	tojson_wlan_config(wlanst, jstart, &jlen);

	SetNumU16(confBuf.wificfgbuf+pos, jlen);
	E(set_config_to_flash(&confBuf));
FIN:
	return err;
}

int set_board_compos(Spi_Ex_Code spiex, Uart_Ex_Code uart1ex)
{
	int err = 0;
	ConfReadBuffer confBuf;
	int pos = strlen(HwTypeConfig_Marker);
	E(pre_set_config(&confBuf));
	memcpy(confBuf.hwtypecfgbuf, HwTypeConfig_Marker, pos);
	confBuf.hwtypecfgbuf[pos++] = spiex;
	confBuf.hwtypecfgbuf[pos++] = uart1ex;
	E(set_config_to_flash(&confBuf));
FIN:
	return err;	
}

int valid_upfw_ftp_params(json_value *pobj, BtParams_ST *btparams)
{
	int pvalint;
	int exflag;
	int tmpmode;
	
	PARSE_CHK_RANGE(pobj, "mode", tmpmode, 1, 2, 1);
	
	if (exflag == 1)
		btparams->updatemode = (FwUpdateModeCode)tmpmode;

	PARSE_CHK_STR(pobj, "server", btparams->ftpaddr, BTFWUPD_FTP_SERADDR_BUFLEN, 1, 0);
	
	PARSE_CHK_STR(pobj, "user", btparams->ftpuser, BTFWUPD_FTP_USER_BUFLEN, 1, 1);
	
	PARSE_CHK_STR(pobj, "pwd", btparams->ftppassword, BTFWUPD_FTP_PASSWORD_BUFLEN, 1, 1);

	PARSE_CHK_STR(pobj, "path", btparams->filename, BTFWUPD_FTP_FILENAME_BUFLEN, 1, 0);

	if (btparams->updatemode == FwUpdateMode_ByFtp_FmEth)
	{
		if (get_spi_ex_dev() != Spi_Ex_Ethernet)
			return -1;
	}
	else if (btparams->updatemode == FwUpdateMode_ByFtp_Fm4G)
	{
		if (get_uart_ex_dev() != Uart_Ex_4G)
			return -1;		
	}
	
	return 0;
}

#endif


