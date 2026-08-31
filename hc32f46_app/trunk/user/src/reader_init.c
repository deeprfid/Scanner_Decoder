#include <string.h>
#include "reader_init.h"
#include "app_conf.h"
#include "hc32f46_driver.h"
#include "reader_msg.h"
#include "ErrChecker.h"
#include "http_callback.h"
#include "mqtt_interface.h"
#include "MQTTClient.h"
#include "mp_pool.h"

int gAntNumber = -1;
int uart0_bauds[] = {115200, 921600, 460800, 9600, 230400, 57600, 38400, 19200};
int uart0_bindex = 0;
char gAddrBuf[20];
ConnAnts_ST gConnants;
int ghReader;
uint8 gIsUnknownMod = 0;
int gCanAsyncInv = 1;

int TagSendBufLen = 1580;
int CmdRecvBufLen = 256;

unsigned char *SockSendBuffer;
unsigned char *SockRecvBuffer;

#define RDR_PARA_OP(pset) \
	do { \
		rerr = pset; \
		if (rerr != MT_OK_ERR) \
		{ \
			TRACE("%s err:%d\n", #pset, rerr); \
			sprintf(gRdrErrStrBuf, "init reader failed:%s", #pset); \
			if (is_ret == 1) \
				return rerr; \
			else \
				is_ret = 1; \
		} \
		else \
			TRACE("%s ok\n", #pset); \
	} while (0)

extern char *gRdrErrStrBuf;
void set_rdr_errstring(char *prefix)
{
	int derrcode;
	char *errstr;
	unsigned char tmpbuf[25];
	
	GetLastDetailError(ghReader, &derrcode, &errstr);
	sprintf(gRdrErrStrBuf, "%s-%s", prefix, errstr);
	if (derrcode == 0x0505 && gPRdrStaSet->tagops_param.inventory.cycle == 0)
	{
		ParamGet(ghReader, MTR_PARAM_READER_ERRORDATA, tmpbuf);
		sprintf(gRdrErrStrBuf+strlen(gRdrErrStrBuf), "-ant_%d", tmpbuf[1]);
	}
}

READER_ERR GetPortsNumber(int hreader)
{
	READER_ERR rerr = MT_OK_ERR;
	unsigned char rdrver[8] = {0};
	int i;
	HardwareDetails hwdetal;
	
	rerr = ParamGet(hreader, MTR_PARAM_READER_VERSION, rdrver);
	if (rerr != MT_OK_ERR)
	{
		set_rdr_errstring("init reader failed");
		CloseReader(ghReader);
		TRACE("MTR_PARAM_READER_VERSION err:%d\n", rerr);
		return rerr;
	}
	TRACE("rdrver:");
	for (i = 0; i < 8; ++i)
		TRACE("%02X ",rdrver[i]);
	TRACE("\n");
	memcpy(gBrdCstDevInfo.modtype, rdrver, 2);
	memcpy(gBrdCstDevInfo.modfwver, rdrver+4, 4);
	
	GetHardwareDetails(hreader, &hwdetal);
	if (hwdetal.module == MODOULE_SLR5100 || 
		hwdetal.module == MODOULE_SLR5200 || 
		hwdetal.module == MODOULE_SLR5300)
		gCanAsyncInv = 0;
	
	gHbData.rfid_mod = hwdetal.module;
	ParamGet(hreader, MTR_PARAM_READER_AVAILABLE_ANTPORTS, &gAntNumber);
	TRACE("!!!!!!!!!!!!!!! rfid_mod:%d, gAntNumber:%d\n", gHbData.rfid_mod, gAntNumber);
	return rerr;
}


READER_ERR OpenReader()
{
	AntPowerConf pwrs;
	int i;
	READER_ERR rerr = MT_OK_ERR;
	int is_ret = 1;
	unsigned short tmpushort;
	int tmpint;
	
	TRACE("power off module now:%lld\n", getSysTick());
	rfid_power_off();
	sleep_ms(1000);
	TRACE("power on module now:%lld\n", getSysTick());
	rfid_power_on();
	sleep_ms(1000);
	TRACE("after power on module now:%lld\n", getSysTick());

	if (gAntNumber == -1)
	{
		for (uart0_bindex = 0; uart0_bindex < 8; ++uart0_bindex)
		{
			sprintf(gAddrBuf, "uart0:%d", uart0_bauds[uart0_bindex]);
			TRACE("addrbuf:%s\n", gAddrBuf);			
			rerr = InitReader(&ghReader, gAddrBuf, MODULE_ONE_ANT);
			if (rerr == MT_OK_ERR)
				break;
			else if (rerr == MT_TEST_DEV_FAULT_5)
			{
				gIsUnknownMod = 1;
				strcpy(gRdrErrStrBuf, "init reader failed:unknown type rfid module");
				return MT_TEST_DEV_FAULT_5;
			}
			else
				TRACE("%s failed\n", gAddrBuf);
		}

		if (rerr != MT_OK_ERR)
		{
			strcpy(gRdrErrStrBuf, "init reader failed:io error");
			TRACE("InitReader err:%d\n", rerr);
			return rerr;
		}
		
		rerr = GetPortsNumber(ghReader);
		if (rerr != MT_OK_ERR)
			return rerr;
	}
	else
	{
		TRACE("before InitReader, gAddrBuf:%s\n", gAddrBuf);
		rerr = InitReader(&ghReader, gAddrBuf, MODULE_ONE_ANT);
		if (rerr != MT_OK_ERR)
		{
			strcpy(gRdrErrStrBuf, "init reader failed:init second io error");
			return rerr;
		}
	}
	
	TRACE("InitReader Ok\n");
	
			dump_static_settings(gPRdrStaSet);
			if (gPRdrStaSet->protocol.gen2.session != -1)
			{
				tmpint = gPRdrStaSet->protocol.gen2.session;
				RDR_PARA_OP(ParamSet(ghReader, MTR_PARAM_POTL_GEN2_SESSION, &tmpint));
			}
			if (gPRdrStaSet->protocol.gen2.q != -2)
			{
				tmpint = gPRdrStaSet->protocol.gen2.q;
				RDR_PARA_OP(ParamSet(ghReader, MTR_PARAM_POTL_GEN2_Q, &tmpint));
			}
			if (gPRdrStaSet->protocol.gen2.target != -1)
			{
				tmpint = gPRdrStaSet->protocol.gen2.target;
				RDR_PARA_OP(ParamSet(ghReader, MTR_PARAM_POTL_GEN2_TARGET, &tmpint));
			}
			if (gPRdrStaSet->protocol.gen2.profile != -1)
			{
//				RDR_PARA_OP(ParamSet(*prdr, MTR_PARAM_POTL_GEN2_TAGENCODING, &gPRdrStaSet->protocol.gen2.profile));
				tmpint = gPRdrStaSet->protocol.gen2.profile;
				ParamSet(ghReader, MTR_PARAM_POTL_GEN2_TAGENCODING, &tmpint);
			}
				
			if (gPRdrStaSet->rf.ant_max_dwell_time > 0)
			{
				is_ret = 0;
				RDR_PARA_OP(ParamSet(ghReader, MTR_PARAM_RF_HOPANTTIME, &gPRdrStaSet->rf.ant_max_dwell_time));				
			}
			if (gPRdrStaSet->rf.hop_mode != -1)
			{
				if (gHbData.rfid_mod == MODOULE_SLR5900 || 
					gHbData.rfid_mod == MODOULE_SLR5800 || 
					gHbData.rfid_mod == MODOULE_SLR6000 || 
					gHbData.rfid_mod == MODOULE_SLR6100 || 
					gHbData.rfid_mod == MODOULE_SLR1100)
				{
					CustomParam_ST cpst;
					char cuscmd[52];
					cpst.pCusParam = cuscmd;
					cpst.CParamlen = 52;
					cuscmd[50] = gPRdrStaSet->rf.hop_mode;						
					strcpy(cuscmd, "rfcommon/hopmode");
					RDR_PARA_OP(ParamSet(ghReader, MTR_PARAM_CUSTOM, &cpst));						
				}
			}
			if (gPRdrStaSet->rf.region != -1)
			{
				Region_Conf tmprg = (Region_Conf)gPRdrStaSet->rf.region;
				RDR_PARA_OP(ParamSet(ghReader, MTR_PARAM_FREQUENCY_REGION, &tmprg));
			}
			if (gPRdrStaSet->rf.hop_table_cnt != 0)
			{
				HoptableData_ST hptab;				
				hptab.lenhtb = gPRdrStaSet->rf.hop_table_cnt;
				for (i = 0; i < hptab.lenhtb; ++i)
					hptab.htb[i] = gPRdrStaSet->rf.hop_table[i];
				
				is_ret = 0;
				RDR_PARA_OP(ParamSet(ghReader, MTR_PARAM_FREQUENCY_HOPTABLE, &hptab));
			}			
	
			RDR_PARA_OP(ParamGet(ghReader, MTR_PARAM_RF_MAXPOWER, &tmpushort));
			pwrs.antcnt = 0;
			for (i = 0; i < gAntNumber; ++i)
			{
				if (gPRdrStaSet->rf.tx_powers[i].read_power != -2)
				{
					if (gPRdrStaSet->rf.tx_powers[i].read_power != -1)	//2025-03-14				
					//	pwrs.Powers[pwrs.antcnt].readPower = gPRdrStaSet->rf.tx_powers[i].read_power<1000 ? 1000: gPRdrStaSet->rf.tx_powers[i].read_power;					
					  pwrs.Powers[pwrs.antcnt].readPower = gPRdrStaSet->rf.tx_powers[i].read_power;
					else
						pwrs.Powers[pwrs.antcnt].readPower = tmpushort;
					
					if (gPRdrStaSet->rf.tx_powers[i].write_power != -1)
					//	pwrs.Powers[pwrs.antcnt].writePower = gPRdrStaSet->rf.tx_powers[i].write_power<1000? 1000:gPRdrStaSet->rf.tx_powers[i].write_power;
					 pwrs.Powers[pwrs.antcnt].writePower = gPRdrStaSet->rf.tx_powers[i].write_power; 
					else
						pwrs.Powers[pwrs.antcnt].writePower = tmpushort;
					
					pwrs.Powers[pwrs.antcnt].antid = i + 1;
					pwrs.antcnt++;
				}
			}

			TRACE("dump AntPowerConf:");
			for (i = 0; i < pwrs.antcnt; ++i)
				TRACE("%d %d %d, ", pwrs.Powers[i].antid, pwrs.Powers[i].readPower, pwrs.Powers[i].writePower);
			TRACE("\n");
			if (pwrs.antcnt > 0)
			{
				RDR_PARA_OP(ParamSet(ghReader, MTR_PARAM_RF_ANTPOWER, &pwrs));
			}

			if (gPRdrStaSet->tag_data.unique_by_antenna != -1)
			{
				tmpint = gPRdrStaSet->tag_data.unique_by_antenna;
				RDR_PARA_OP(ParamSet(ghReader, MTR_PARAM_TAGDATA_UNIQUEBYANT, &tmpint));
			}
			if (gPRdrStaSet->tag_data.unique_by_bank_data != -1)
			{
				tmpint = gPRdrStaSet->tag_data.unique_by_bank_data;
				RDR_PARA_OP(ParamSet(ghReader, MTR_PARAM_TAGDATA_UNIQUEBYEMDDATA, &tmpint));
			}
			if (gPRdrStaSet->tag_data.record_highest_rssi != -1)
			{
				tmpint = gPRdrStaSet->tag_data.record_highest_rssi;
				RDR_PARA_OP(ParamSet(ghReader, MTR_PARAM_TAGDATA_RECORDHIGHESTRSSI, &tmpint));
			}

		if (gPRdrStaSet->tagops_param.inventory.inv_mode != -1)
		{
			if (gPRdrStaSet->tagops_param.inventory.cycle == 0)
			{
				char exfmodegpara[75];
				CustomParam_ST cpmst;

				memset(exfmodegpara, 0, 75);
				strcpy(exfmodegpara, "Reader/Ex10fastmode");					
				cpmst.pCusParam = exfmodegpara;
				exfmodegpara[51] = 20;
				exfmodegpara[52] = 0;
				cpmst.CParamlen = 22;
				if (gPRdrStaSet->tagops_param.inventory.inv_mode == 0)
					exfmodegpara[50] = 0;
				else
					exfmodegpara[50] = 1;
				TRACE("set Reader/Ex10fastmode:%d\n", exfmodegpara[50]);
				ParamSet(ghReader, MTR_PARAM_CUSTOM, &cpmst);
			}
		}
		
		if (gPRdrStaSet->tagops_param.bankdata.is_bankdata == 1)
		{
			EmbededData_ST emd;
			emd.bank = gPRdrStaSet->tagops_param.bankdata.bank;
			emd.startaddr = gPRdrStaSet->tagops_param.bankdata.start;
			emd.bytecnt = gPRdrStaSet->tagops_param.bankdata.blkcnt*2;
			if (GetNumU32(gPRdrStaSet->tagops_param.accessop.aespwd))
				emd.accesspwd = NULL;
			else
				emd.accesspwd = gPRdrStaSet->tagops_param.accessop.aespwd;
			ParamSet(ghReader, MTR_PARAM_TAG_EMBEDEDDATA, &emd);
		}

		if (gPRdrStaSet->tagops_param.tagfilter.is_tagfilter == 1)
		{
			TagFilter_ST filter;
			filter.bank = gPRdrStaSet->tagops_param.tagfilter.bank;
			filter.startaddr = gPRdrStaSet->tagops_param.tagfilter.start;
			filter.isInvert = 1- gPRdrStaSet->tagops_param.tagfilter.match;
			filter.fdata = gPRdrStaSet->tagops_param.tagfilter.mask;
			filter.flen = gPRdrStaSet->tagops_param.tagfilter.mask_len;
			ParamSet(ghReader, MTR_PARAM_TAG_FILTER, &filter);
		}
		
		{
			int transtime = 500;
			ParamSet(ghReader, MTR_PARAM_TRANS_TIMEOUT, &transtime);
		}

	if (gCurWorkMode == WorkMode_Passive)
		return rerr;

	if (gAntNumber != 1)
	{
		gConnants.antcnt = gPRdrStaSet->tagops_param.inventory.ants_cnt;
		for (i = 0; i < gConnants.antcnt; ++i)
			gConnants.connectedants[i] = gPRdrStaSet->tagops_param.inventory.ants[i];

		if (gConnants.antcnt != 0)
			gHbData.antcount = 0;
		else 
		{
			AntPortsVSWR vswr;
			for (i = 0; i < gAntNumber; ++i)
			{
				vswr.frecount = 3;
				vswr.power = 2300;
				vswr.region = RG_NA;
				vswr.frequencys[0] = 915250;
				vswr.frequencys[1] = 903250;
				vswr.frequencys[2] = 926750;
				vswr.andid = i + 1;
						
				rerr = ParamGet(ghReader, MTR_PARAM_RF_ANTPORTS_VSWR, &vswr);
				if (rerr != MT_OK_ERR)
				{
					set_rdr_errstring("init reader failed");
					CloseReader(ghReader);
					TRACE("error when ParamGet MTR_PARAM_RF_ANTPORTS_VSWR:%d, i:%d\n", rerr, i);
					return rerr;
				}
				else
				{
					if ((vswr.vswrs[0] > 25) && (vswr.vswrs[1] > 25) && (vswr.vswrs[2] > 25))
					{
						TRACE("find ant %d\n", i + 1);
						gConnants.connectedants[gConnants.antcnt] = i + 1;
						gConnants.antcnt++;
								
					}
				}
			}
				
			if (gConnants.antcnt == 0)
			{
				CloseReader(ghReader);
				strcpy(gRdrErrStrBuf, "init reader failed: no antennas found");
				TRACE("MT_HARDWARE_ALERT_ERR_BY_NO_ANTENNAS\n");
				return MT_HARDWARE_ALERT_ERR_BY_NO_ANTENNAS;
			}
			else
				gHbData.antcount = gConnants.antcnt;

			for (i = 0; i < gHbData.antcount; ++i)
				gHbData.connected_antennas[i] = gConnants.connectedants[i];
		}	
	}
	else
	{
		gHbData.antcount = 0;
		gConnants.antcnt = 1;
		gConnants.connectedants[0] = 1;
	}		
	
	TRACE("gConnants.antcnt:%d", gConnants.antcnt);
	for (i = 0; i < gConnants.antcnt; ++i)
		TRACE(" %d", gConnants.connectedants[i]);
	TRACE("\n");
	
	return rerr;
}


int HandleModErr()
{
	int i;
	
	TRACE("enter HandleModErr\n");
	
	if (echr_istrigger() == 1)
	{
		TRACE("if (echr_istrigger() == 1)\n");
		gErrSend = gRdrErr;
		set_rdr_errstring("inv tags");
		return -1;
	}

	CloseReader(ghReader);
	
	for (i = 0; i < 3; ++i)
	{
		gRdrErr = OpenReader();
		if (gRdrErr == MT_OK_ERR)
			break;
	}
	gIsStartAsyncInv = 0;
	gErrSend = gRdrErr;
	if (gRdrErr == MT_OK_ERR)
	{
//		get_left_heap_size("HandleModErr");
		return 0;
	}
	else
	{
		TRACE("HandleModErr failed:%d\n", gRdrErr);
		if (gRdrErr == MT_HARDWARE_ALERT_ERR_BY_NO_ANTENNAS)
			led_toggle(-1, 1000, NULL);
		else
			led_toggle(-1, 200, NULL);
		return -1;
	}
}

extern MQTTPacket_connectData gConData;
extern Network gNwkMqtt;
uint8 *gMqttSendBuf;
void (*g_wg_send_fn)(uint8, uint8, uint8 *);
uint8 gWgGytes;

int init_upload(void)
{
	int ret;
	commonUartPara uartPara;
	int tagbufsize;
	int isu_ant;
	int isu_bank;
	int isr_rssi;
	int baud = 115200;
	
//	if (uart0_bauds[uart0_bindex] > 115200)
//		baud = 460800;
//	else
//		baud = 115200;
	
	memset(&uartPara, 0, sizeof(commonUartPara));
	uartPara.baudrate	= gPRdrStaSet->uart1.baud;
	uartPara.databits = gPRdrStaSet->uart1.data_bits;
	uartPara.stopbits = gPRdrStaSet->uart1.stop_bits;
	uartPara.parity = gPRdrStaSet->uart1.parity;
	uartPara.flowctrl = gPRdrStaSet->uart1.flow_ctrl;
	uartPara.timeout	= 1000;
	uart_open(COMMON_INTERFACE_UART2,&uartPara);	
	
	memset(&uartPara, 0, sizeof(commonUartPara));
	uartPara.baudrate	= gPRdrStaSet->uart2.baud;
	uartPara.databits = gPRdrStaSet->uart2.data_bits;
	uartPara.stopbits = gPRdrStaSet->uart2.stop_bits;
	uartPara.parity = gPRdrStaSet->uart2.parity;
	uartPara.flowctrl = gPRdrStaSet->uart2.flow_ctrl;
	uartPara.t485 = 1;
	uartPara.timeout	= 1000;
	uart_open(COMMON_INTERFACE_UART3,&uartPara);	
		
	gHbData.main_board = 1;
	firmware_version(gHbData.software_version);
	gHbData.hb_count = 0;
	
	if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Mqtt)
		CmdRecvBufLen = MqttRecvBufLen;
	
	TagSendBufLen = gRtSetting->glob_params.s_buf_size;
	
	SockSendBuffer = malloc_hexp(TagSendBufLen);
	SockRecvBuffer = malloc_hexp(CmdRecvBufLen);
	
	if(gRtSetting->upload.hw_inf == Upload_Inf_Wiegand)
	{
		if (gRtSetting->upload.sw_potl_params.wiegand.type == Wg_Type_26)
		{
			g_wg_send_fn = wiegand_send26;
			gWgGytes = 3;
		}
		else 	if (gRtSetting->upload.sw_potl_params.wiegand.type == Wg_Type_34)
		{
			g_wg_send_fn = wiegand_send34;
			gWgGytes = 4;
		}
		else 	if (gRtSetting->upload.sw_potl_params.wiegand.type == Wg_Type_66)
		{
			g_wg_send_fn = wiegand_send66;
			gWgGytes = 8;
		}
	}
	else if (gRtSetting->upload.hw_inf == Upload_Inf_4G || 
		gRtSetting->upload.hw_inf == Upload_Inf_Ethernet || 
		gRtSetting->upload.hw_inf == Upload_Inf_Wifi)
	{
		char domain[100];
//		InitDegutPrintf(1, NULL, 0);
		
		if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Tcp)
		{
			strcpy(domain, gRtSetting->upload.sw_potl_params.tcp.ser_ip);
			gSerPort = gRtSetting->upload.sw_potl_params.tcp.ser_port;
		}
		else if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Http)
		{
			init_http_fn();
			ret = url_get_domain(gRtSetting->upload.sw_potl_params.http.url, 
				domain, &gSerPort, &gAbsPathpos, &gIsTlsConn);
			if (ret != 0)
			{
				TRACE("!!!!!!!!!!!!!!!!!!! url_get_domain error\n");
				return -1;
			}
		}
		else if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Mqtt)
		{
			strcpy(domain, gRtSetting->upload.sw_potl_params.mqtt.host);
			gSerPort = gRtSetting->upload.sw_potl_params.mqtt.port;
			gIsTlsConn = gRtSetting->upload.sw_potl_params.mqtt.tls;
			
			if (gRtSetting->upload.hw_inf == Upload_Inf_Ethernet)				
				NewNetwork(&gNwkMqtt, COMMON_INTERFACE_SOCKET0, gIsTlsConn);
			else
				NewNetwork(&gNwkMqtt, COMMON_INTERFACE_UART1, gIsTlsConn);
			
			gConData.willFlag = 0;
			gConData.MQTTVersion = 4;
			gConData.clientID.cstring = (char *)gRtSetting->glob_params.name;
			gConData.username.cstring = gRtSetting->upload.sw_potl_params.mqtt.user;
			gConData.password.cstring = gRtSetting->upload.sw_potl_params.mqtt.pwd;
			gConData.keepAliveInterval = gRtSetting->upload.sw_potl_params.mqtt.kal_time;
			gConData.cleansession = 1;
			gMqttSendBuf = malloc_hexp(TagSendBufLen+100);			
		}
		
		if (addr_str2bin(domain, gSerIp) != 0 && 
			gRtSetting->upload.hw_inf == Upload_Inf_Ethernet)
		{
			int failcnt = 0;
			wiz_NetInfo info;
			TRACE("run dns for server ip\n");
			pre_DNS_init();
			DNS_init(getMaxSocketId(), SockSendBuffer);
			wizchip_getnetinfo(&info);			
			TRACE("dnsServer:%d.%d.%d.%d\n", info.dns[0], 
				info.dns[1], info.dns[2], info.dns[3]);
			while (1)
			{
				ret =  DNS_run(info.dns, (uint8 *) domain, gSerIp);
				if (ret == 1)
				{
					aft_DNS_run();
					TRACE("get serip:%d.%d.%d.%d\n", gSerIp[0], 
						gSerIp[1], gSerIp[2], gSerIp[3]);
					break;
				}
				else
				{
					failcnt++;
					TRACE("Dns parse failed %d times\n", failcnt);
					sleep_ms(10000);
				}
			}	
		}
		
		if (gIsTlsConn == 1)
		{
			preinit_mbedtls();
			if (gRtSetting->upload.hw_inf == Upload_Inf_Ethernet)
				gMbedNetFd = COMMON_INTERFACE_SOCKET0;
			else
				gMbedNetFd = COMMON_INTERFACE_UART1;
		}
		
		if (gRtSetting->upload.hw_inf == Upload_Inf_4G)
		{
			if (init_4g(baud, domain, gSerPort, 0, 0) == 0)
			{
				TRACE("init_4g ok\n");
//				g4_reconn_ser();
			}
			else
			{
				TRACE("init_4g failed\n");
				return -1;
			}
		}
		else if (gRtSetting->upload.hw_inf == Upload_Inf_Wifi)
		{
			if (init_wlan(baud, domain, gSerPort, 0, 0) == 0)
			{
				if (gRtSetting->glob_params.name[0] == 0)
				{
					sprintf(gRtSetting->glob_params.name, "%02X%02X%02X%02X%02X%02X", 
						gWlanNet.mac[0], gWlanNet.mac[1], gWlanNet.mac[2], gWlanNet.mac[3], 
						gWlanNet.mac[4], gWlanNet.mac[5]);
				}
				TRACE("init_wlan ok\n");
			}
			else
			{
				TRACE("init_wlan failed\n");
				return -1;
			}
		}
	}
	
	tagbufsize = get_left_heap_size("tagbufsize") - DynMemReserveSize - ReaderCppMemSize;
	if (gIsTlsConn == 1)
		tagbufsize -= MbedTLSDynMemSize;
	tagbufsize += 8 - tagbufsize %8;
	TRACE("tagbufsize:%d\n", tagbufsize);
	initTbBuffer(gPRdrStaSet->app_init.max_tb_rec_len, tagbufsize);
	get_left_heap_size("after initTbBuffer");
	if (gIsTlsConn == 1)
		init_mbedtls();
	
	get_left_heap_size("after init_mbedtls");
	
	if (gPRdrStaSet->tag_data.unique_by_antenna == -1)
		isu_ant = 0;
	else
		isu_ant = gPRdrStaSet->tag_data.unique_by_antenna;
	
	if (gPRdrStaSet->tag_data.unique_by_bank_data == -1)
		isu_bank = 1;
	else
		isu_bank = gPRdrStaSet->tag_data.unique_by_bank_data;
	
	if (gPRdrStaSet->tag_data.record_highest_rssi == -1)
		isr_rssi = 0;
	else
		isr_rssi = gPRdrStaSet->tag_data.record_highest_rssi;
	
	setUniByAnt(isu_ant);
	setUniByEmdData(isu_bank);
	setRecHighestRssi(isr_rssi);
	
#if Custom_By_Caipan
	if (gRtSetting->upload.hw_inf == Upload_Inf_Wifi)
	{
		WlanConfig_ST wlanst;
		get_wlan_config(&wlanst);
		
		sprintf(gMsg_IPStr, "%d.%d.%d.%d", wlanst.ipinfo.ip[0], wlanst.ipinfo.ip[1], 
			wlanst.ipinfo.ip[2], wlanst.ipinfo.ip[3]);
		sprintf(gMsg_MACStr, "%02X:%02X:%02X:%02X:%02X:%02X", wlanst.ipinfo.mac[0], wlanst.ipinfo.mac[1], 
			wlanst.ipinfo.mac[2], wlanst.ipinfo.mac[3], wlanst.ipinfo.mac[4], wlanst.ipinfo.mac[5]);
	}		
	else
	{
		sprintf(gMsg_IPStr, "%d.%d.%d.%d", gNetConf.ip[0], gNetConf.ip[1], 
			gNetConf.ip[2], gNetConf.ip[3]);		
		sprintf(gMsg_MACStr, "%02X:%02X:%02X:%02X:%02X:%02X", gNetConf.mac[0], gNetConf.mac[1], 
			gNetConf.mac[2], gNetConf.mac[3], gNetConf.mac[4], gNetConf.mac[5]);
	}
#endif		
	return 0;
}




