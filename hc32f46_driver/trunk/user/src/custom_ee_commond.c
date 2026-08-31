#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "hc32_ddl.h"
#include "hc32f46_driver.h"
#include "driverconfig.h"

#if IS_RTOS2_SUPPORT

EERCmdGpoSet_ST gEEcmdGpoSet;
extern BoardComponents_ST gBoardCompos;

void cus_ee_short_reply(int fd, uint8* sbuf, uint8 cid, uint32 ecode, int iscrc, 
	int (* sendfunc)(int, uint8 *, int), uint16 (* crc_Msg)(uint8 *, int), 
		ReaderRunTimeSettings_ST *prtset, uint8 *data, int dlen)
{
	int pos = 0;
	uint8 *geterrbuf = sbuf;
	uint16 crc;
	int namelen;
	
	geterrbuf[pos++] = 0xff;
	if (prtset != NULL)
	{
		namelen = strlen(prtset->glob_params.name);
		geterrbuf[pos++] = namelen;
	}
	else
		geterrbuf[pos++] = 0x00;
	SetNumU16(geterrbuf+pos, dlen);
	pos += 2;
	geterrbuf[pos++] = cid;
	if (iscrc == 0)
		geterrbuf[pos++] = 0x00;
	else
		geterrbuf[pos++] = 0x02;
	SetNumU32(geterrbuf+pos, ecode);
	pos += 4;

	if (prtset != NULL)
	{
		memcpy(geterrbuf+pos, prtset->glob_params.name, namelen);
		pos += namelen;
	}
	if (dlen > 0)
	{
		memcpy(geterrbuf+pos, data, dlen);
		pos += dlen;
	}
	if (iscrc == 1)
	{
		crc = crc_Msg(geterrbuf, pos);
		SetNumU16(geterrbuf+pos, crc);
		pos += 2;
	}
	sendfunc(fd, geterrbuf, pos);	
}

extern const char *ACPMagicStr;

int custom_ee_commond(int rfd, unsigned char *buf, int mode, 
	int (* save_cur_set)(void), int (* sendfunc)(int, uint8 *, int), 
		uint16 (* crc_msg)(uint8 *, int), ReaderRunTimeSettings_ST *prtset)
{
	int datalen = (buf[2] << 8) | buf[3];
	int iscrc;
	int leftbytes;
	uint8 *pData = NULL;
	uint8 *pMsg;
	int ret;
	uint16 crc;
	uint32 ecode = 0;
	int reboot = 0;
	int detecthwext = 0;
	int namelen;
	
//	printf("111111 custom_ee_commond mode:%d\n", mode);
	iscrc = (buf[5] >> 1) & 0x01;
	if (iscrc == 1)
		leftbytes = 2+datalen;
	else
		leftbytes = datalen;
	
//	printf("222222 custom_ee_commond leftbytes:%d\n", leftbytes);
	if (leftbytes > 1800)
		return -1;
	if (leftbytes > 80)
	{
		pData = malloc_hexp(leftbytes+6);
		pMsg = pData;
		memcpy(pMsg, buf, 6);
	}
	else
		pMsg = buf;
	
	if (leftbytes > 0)
	{
		ret = read_n(rfd, pMsg+6, leftbytes);
		if (ret <= 0)
		{
			if (pData != NULL)
				free_hexp(pData);
			TRACE("33333 custom_ee_commond ret:%d\n", ret);
			return -1;
		}
		if (iscrc == 1)
		{
			crc = crc_msg(pMsg, datalen+6);
			if (crc != ((pMsg[datalen+6] << 8) | pMsg[datalen+7]))
			{
				TRACE("crc check error crc:%04X, crc_:%02X%02X\n", crc, pMsg[datalen+6], 
					pMsg[datalen+7]);
				if (pData != NULL)
					free_hexp(pData);
				return -1;
			}		
		}
	}
//	printf("4444 custom_ee_commond cmdid:%d\n", buf[4]);
	if (mode == 3)
		namelen = strlen(prtset->glob_params.name);
	
	switch (buf[4])
	{
		case MidMsgType_GetStaticConf:
		case MidMsgType_GetRunTimeConf:
		{
			uint8 *jbuf;
			int jlen;
			int jpos = 0;
			int i;
			int scnt;
			int lastbytes;
			void *pRdrSet;
			int confpos = 10;
			WorkMode_Code wcode;
			
			if (mode == 3)
				confpos += namelen;
			
			wcode = TestFwType_ex();
			if (wcode == WorkMode_ActVer_1)
			{
				ecode = 6;
				break;
			}
			
			jbuf = malloc_hexp(2048);
			if (buf[4] == MidMsgType_GetStaticConf)
			{				
				TRACE("case MidMsgType_GetStaticConf:\n");
				pRdrSet = malloc_hexp(sizeof(ReaderStaticSettings_ST));
				get_rdr_static_settings(pRdrSet);
//				dump_static_settings(pRdrSet);
				
				tojson_rdr_static_settings(pRdrSet, (char *)jbuf + confpos, 
					&jlen, 1);	
			}
			else if (buf[4] == MidMsgType_GetRunTimeConf)
			{
				TRACE("case MidMsgType_GetRunTimeConf:\n");
				pRdrSet = malloc_hexp(sizeof(ReaderRunTimeSettings_ST));								
				get_rdr_runtime_settings(pRdrSet);
				tojson_rdr_runtime_settings(pRdrSet, (char *)jbuf + confpos, &jlen);
			}
			
			jbuf[jpos++] = 0xff;
			if (mode == 3)
				jbuf[jpos++] = namelen;
			else
				jbuf[jpos++] = 0x00;
			SetNumU16(jbuf+jpos, jlen);
			jpos += 2;
			jbuf[jpos++] = buf[4];
			if (iscrc == 0)
				jbuf[jpos++] = 0x00;
			else
				jbuf[jpos++] = 0x02;
			SetNumU32(jbuf+jpos, 0);
			jpos += 4;
			if (mode == 3)
			{
				memcpy(jbuf+jpos, prtset->glob_params.name, namelen);
				jpos += namelen;
			}
			jpos += jlen;
			
			if (iscrc == 1)
			{
				crc = crc_msg(jbuf, jpos);
				SetNumU16(jbuf+jpos, crc);
				jpos += 2;
				TRACE("crc:%04X\n", crc);
			}

			scnt = jpos / 256;
			lastbytes = jpos % 256;
			for (i = 0; i < scnt; ++i)
			{
				sendfunc(rfd, jbuf+256*i, 256);
				sleep_ms(5);
			}
			if (lastbytes > 0)
				sendfunc(rfd, jbuf+256*i, lastbytes);
				
			free_hexp(jbuf);
			free_hexp(pRdrSet);
			return 0;
		}
		case MidMsgType_SetStaticConf:
		case MidMsgType_SetRunTimeConf:
		{
//			void *pRdrSet;
			int validret;
			WorkMode_Code wcode;
			////
//			pMsg[6+datalen] = 0;
//			printf("rawjson:%s\n", (char *)pMsg+6);
			////
			wcode = TestFwType_ex();
			if (wcode == WorkMode_ActVer_1)
			{
				ecode = 6;
				break;
			}
			if (buf[4] == MidMsgType_SetStaticConf)
			{
				validret = cmd_config_static_settings(NULL, NULL, (char *)pMsg+6, datalen, &reboot);
				/*
				ReaderStaticSettings_ST *rssettings;
				pRdrSet = malloc_hexp(sizeof(ReaderStaticSettings_ST));
				rssettings = pRdrSet;
				validret = valid_static_settings((char *)pMsg+6, datalen, pRdrSet);
				if (validret == 0)
				{
					setdef_rdr_static_settings(pRdrSet);
					if (rssettings->reset == 0)
						get_rdr_static_settings(pRdrSet);

					valid_static_settings((char *)pMsg+6, datalen, pRdrSet);	
					dump_static_settings(rssettings);
					
					if (rssettings->ethernet.is_netset == 1)
					{
						TRACE("if (rssettings->ethernet.is_netset == 1)\n");
						set_network_config(&rssettings->ethernet.netsettings);
						sleep_ms(150);
					}
					if (rssettings->wlan.is_wlanset == 1)
					{
						TRACE("if (rssettings->wlan.is_wlanset == 1)\n");
						set_wlan_config(&rssettings->wlan.wlansettings);
						sleep_ms(150);
					}
					if (rssettings->ble.is_bleset == 1)
					{
						TRACE("if (rssettings->ble.is_bleset == 1)\n");
						set_bluetooth_config(&rssettings->ble.blesettings);
						sleep_ms(150);
					}
					
					set_rdr_static_settings(rssettings);
					reboot = rssettings->reboot;
					TRACE("reboot:%d\n", reboot);
				}*/
			}
			else if (buf[4] == MidMsgType_SetRunTimeConf)
			{
				validret = cmd_config_runtime_settings(NULL, NULL, (char *)pMsg+6, datalen);
				if (validret == 0)
					reboot = 1;
				/*
				ReaderRunTimeSettings_ST *rrtsettings;
				pRdrSet = malloc_hexp(sizeof(ReaderRunTimeSettings_ST));
				rrtsettings = pRdrSet;
				
				validret = valid_runtime_settings((char *)pMsg+6, datalen, pRdrSet);
				if (validret == 0)
				{
					setdef_rdr_runtime_settings(pRdrSet);
					if (rrtsettings->reset == 0)
						get_rdr_runtime_settings(pRdrSet);

					valid_runtime_settings((char *)pMsg+6, datalen, pRdrSet);
					dump_runtime_settings(pRdrSet);
					if (rrtsettings->upload.hw_inf == 0)					
						validret = -1;
					else
					{
						if (Upload_Inf_Ethernet == rrtsettings->upload.hw_inf)
						{
							if (get_spi_ex_dev() != Spi_Ex_Ethernet)
								validret = -1;
						}
						else if (Upload_Inf_4G == rrtsettings->upload.hw_inf)
						{
							if (get_uart1_ex_dev() != Uart1_Ex_4G)
								validret = -1;
						}
						else if (Upload_Inf_Wifi == rrtsettings->upload.hw_inf)
						{
							if (get_uart1_ex_dev() != Uart1_Ex_Wlan)
								validret = -1;
						}						
						if (validret == 0)
						{
							set_rdr_runtime_settings(pRdrSet);
							sleep_ms(200);
							set_workmode_params(WorkMode_ActVer_2);
							reboot = 1;
						}
					}
				}*/
			}
			
			if (validret != 0)
			{
				TRACE("valid_settings != 0\n");
				ecode = 0x07;
			}
//			free_hexp(pRdrSet);		
			break;
		}
		/*
		case MidMsgType_GetWlanConf:
		case MidMsgType_GetBluetoothConf:
		{
			uint8 *jbuf = NULL;
			int jlen;
			int jpos = 0;
			void *exconf = NULL;
//			WlanConfig_ST *wlanconf;
			
			if (mode == 3)
				ecode = 18;
			else
			{
				jbuf = malloc_hexp(256);
				if (buf[4] == MidMsgType_GetWlanConf)
				{
					TRACE("case MidMsgType_GetWlanConf:\n");
					if (gBoardCompos.uart1_ex != Uart1_Ex_Wlan)
						ecode = 0x06;
					else
					{
						exconf = malloc_hexp(sizeof(WlanConfig_ST));
						get_wlan_config(exconf);
						tojson_wlan_config(exconf, (char *)jbuf + 10, &jlen);
					}
				}
				else if (buf[4] == MidMsgType_GetBluetoothConf)
				{
					TRACE("case MidMsgType_GetBluetoothConf:\n");
					if (gBoardCompos.uart1_ex != Uart1_Ex_Bluetooth)
						ecode = 0x06;
					else
					{
						exconf = malloc_hexp(sizeof(BluetoothConfig_ST));
						get_bluetooth_config(exconf);
						tojson_bluetooth_config(exconf, (char *)jbuf + 10, &jlen);
					}					
				}
				if (ecode == 0x00)
				{
					jbuf[jpos++] = 0xff;
					jbuf[jpos++] = 0x00;
					SetNumU16(jbuf+jpos, jlen);
					jpos += 2;
					jbuf[jpos++] = buf[4];
					if (iscrc == 0)
						jbuf[jpos++] = 0x00;
					else
						jbuf[jpos++] = 0x02;
					SetNumU32(jbuf+jpos, 0);
					jpos += 4;
					jpos += jlen;
					
					if (iscrc == 1)
					{
						crc = crc_msg(jbuf, jpos);
						SetNumU16(jbuf+jpos, crc);
						jpos += 2;
						TRACE("crc:%04X\n", crc);
					}
					
					sendfunc(rfd, jbuf, jpos);
				}
				
				if (jbuf != NULL)
					free_hexp(jbuf);
				if (exconf != NULL)
					free_hexp(exconf);
				
				if (ecode == 0x00)
					return 0;
			}
			break;
		}
		case MidMsgType_SetWlanConf:
		case MidMsgType_SetBluetoothConf:
		{
			void *exconf = NULL;
			int validret;
			if (mode == 3)
				ecode = 18;
			else 
			{
				if (buf[4] == MidMsgType_SetWlanConf)
				{
					if (gBoardCompos.uart1_ex != Uart1_Ex_Wlan)
						ecode = 0x06;
					else
					{
						exconf = malloc_hexp(sizeof(WlanConfig_ST));
						setdef_wlan_config(exconf);
						validret = valid_wlan_config((char *)pMsg+6, datalen, exconf);
						if (validret == 0)
						{
							set_wlan_config(exconf);
							reboot = 1;
						}
						else
						{
							TRACE("valid_wlan_config != 0\n");
							ecode = 0x07;
						}
					}
				}
				else if (buf[4] == MidMsgType_SetBluetoothConf)
				{
					if (gBoardCompos.uart1_ex != Uart1_Ex_Bluetooth)
						ecode = 0x06;
					else
					{
						exconf = malloc_hexp(sizeof(BluetoothConfig_ST));
						setdef_bluetooth_config(exconf);
						validret = valid_bluetooth_config((char *)pMsg+6, datalen, exconf);
						if (validret == 0)
						{
							set_bluetooth_config(exconf);
							reboot = 1;
						}
						else
						{
							TRACE("valid_bluetooth_config != 0\n");
							ecode = 0x07;
						}
					}					
				}
			}
			
			if (exconf != NULL)
				free_hexp(exconf);
			break;
		}*/
		case MidMsgType_EraseReaderConf:
		{
			TRACE("case MidMsgType_EraseReaderConf:B1:%02X, B2:%02X\n", pMsg[6], pMsg[7]);
			erase_multi_config((pMsg[6] << 8) | pMsg[7]);
			reboot = 1;
//			cus_ee_short_reply(buf[4], ecode, iscrc, sendfunc);
			break;
		}
		case MidMsgType_SaveCurStaticConf:
		{
			if (mode == 1)
				ecode = save_cur_set();
			else
				ecode = 0x06;
			break;
		}
		case MidMsgType_TestUart1ex:
		{
			if (mode == 1)
			{
				if (pMsg[6] == Uart_Ex_4G)
				{
					if (get_uart_ex_dev() == Uart_Ex_4G)
					{
						if (check_4g(115200) != 0)
							ecode = 0x03;
					}
					else
						ecode = 0x06;
				}
				else if (pMsg[6] == Uart_Ex_Wlan)
				{
					char ssid[30];
					char pwd[30];
					int slen;
					int plen;
					int testpos = 7;
					
					//ssidlen,ssid,pwdlen,pwd
					slen = pMsg[testpos++];
					memcpy(ssid, pMsg+testpos, slen);
					testpos += slen;
					ssid[slen] = 0;
					plen = pMsg[testpos++];;
					memcpy(pwd, pMsg+testpos, plen);
					pwd[plen] = 0;
					if (check_wlan(115200, ssid, pwd) != 0)
						ecode = 0x03;
				}
				else
					ecode = 0x07;
			}
			else
				ecode = 0x06;
			break;
		}
		case MidMsgType_Reboot:
		{
			reboot = 1;
			break;
		}
		case MidMsgType_GetConf:
		{
			uint8 SendBuffer[32];
			int datalen;
			int lastnbytes;
			int i;
			int pos = 0;
			uint8 buf3bytes[3];
			WorkMode_Code wcode;
			TRACE("case MidMsgType_GetConf\n");
			wcode = TestFwType_ex();
			if (wcode != WorkMode_ActVer_1)
			{
				ecode = 6;
				break;
			}
			
			flash_bytes_read(ActiveModeConfig_Addr, SendBuffer, 20);
			if (memcmp(SendBuffer, ACPMagicStr, 15) != 0)
				ecode = 0x12;
			else
			{
				memcpy(buf3bytes, SendBuffer+17, 3);
				
				SendBuffer[pos++] = 0xff;
				if (mode == 3)
					SendBuffer[pos++] = namelen;
				else
					SendBuffer[pos++] = 0x00;
				datalen = GetNumU16(SendBuffer+15);
				SetNumU16(SendBuffer+pos, datalen+18);
		//			printf("datalen:%d\n", datalen);
				pos += 2;
				SendBuffer[pos++] = 20;
				SendBuffer[pos++] = 0x00;
				SendBuffer[pos++] = 0x00;
				SendBuffer[pos++] = 0x00;
				SendBuffer[pos++] = 0x00;
				SendBuffer[pos++] = 0x00;
				if (mode == 3)
				{
					memcpy(SendBuffer+pos, prtset->glob_params.name, namelen);
					pos += namelen;
				}
				memcpy(SendBuffer+pos, gNetConf.ip, 4);
				pos += 4;
				memcpy(SendBuffer+pos, gNetConf.subnetMask, 4);
				pos += 4;
				memcpy(SendBuffer+pos, gNetConf.gatewayIP, 4);
				pos += 4;
				memcpy(SendBuffer+pos, gNetConf.mac, 6);
				pos += 6;
				memcpy(SendBuffer+pos, buf3bytes, 3);
				pos += 3;
				
				if (sendfunc(rfd, SendBuffer, pos) != 0)
					return -1;
				
				datalen -= 3;
				for (i = 0; i < datalen / 32; ++i)
				{
					flash_bytes_read(ActiveModeConfig_Addr+20+i*32, SendBuffer, 32);
					if (sendfunc(rfd, SendBuffer, 32) != 0)
						return -1;
				}
				
				lastnbytes = datalen % 32;
				if (lastnbytes != 0)
				{
					int realnum = lastnbytes;
					if (lastnbytes % 4 != 0)
						realnum = lastnbytes + (4 - lastnbytes % 4);
					flash_bytes_read(ActiveModeConfig_Addr+20+i*32, SendBuffer, realnum);
					if (sendfunc(rfd, SendBuffer, lastnbytes) != 0)
						return -1;			
				}				
			}
			return 0;
		}
		case MidMsgType_SetConf:
		{
			if (datalen == 0)
			{
				TRACE("case MidMsgType_SetConf -erase_multi_config\n");
				erase_multi_config(ERASE_FLS_CFG_BIT_ACTMODE);
				reboot = 1;
			}
			else
			{
				TRACE("case MidMsgType_SetConf -set conf\n");
				if (SetFlashConfig(pMsg+6, datalen, &gNetConf) < 0)
					ecode = 0x07;
				else
					reboot = 1;				
			}
			break;
		}
		case MidMsgType_GetCurWorkMode:
		{
			if (mode == 3)
				ecode = 18;
			else
			{
				uint8 workstate[1];
				if (gBrdCstDevInfo.workmode == 2)
					workstate[0] = TestFwType_ex();
				else
					workstate[0] = WorkMode_Passive;
				cus_ee_short_reply(rfd, buf, buf[4], ecode, iscrc, sendfunc, crc_msg, prtset, workstate, 1);
				return 0;
			}
			break;
		}
		case MidMsgType_SwitchWorkMode:
		{
			WorkMode_Code wcode = TestFwType_ex();
			if (wcode == WorkMode_ActVer_1)
			{
				ecode = 6;
				break;
			}
			
			if (pMsg[6] == WorkMode_Passive || pMsg[6] == WorkMode_ActVer_2)
			{
				if (pMsg[6] == WorkMode_ActVer_2)
				{
					ReaderRunTimeSettings_ST *rtsetnow = malloc_hexp(sizeof(ReaderRunTimeSettings_ST));
					setdef_rdr_runtime_settings(rtsetnow);
					get_rdr_runtime_settings(rtsetnow);
					if (check_runtime_settings(rtsetnow) != 0)
						ecode = 18;
					free_hexp(rtsetnow);
				}
			}
			else
				ecode = 7;
			
			if (ecode == 0)
			{
				set_workmode_params((WorkMode_Code)pMsg[6]);
				reboot = 1;
			}
			break;
		}
		case MidMsgType_GetGPI:
		{
			uint8 gpistats[9];
			int ggpos = 0;
			
			gpistats[ggpos++] = 4;
			gpistats[ggpos++] = 1;
			gpistats[ggpos++] = gpi_get(1);
			gpistats[ggpos++] = 2;
			gpistats[ggpos++] = gpi_get(2);
			gpistats[ggpos++] = 3;
			gpistats[ggpos++] = gpi_get(3);
			gpistats[ggpos++] = 4;
			gpistats[ggpos++] = gpi_get(4);
			cus_ee_short_reply(rfd, buf, buf[4], ecode, iscrc, sendfunc, crc_msg, prtset, gpistats, ggpos);
			return 0;
		}
		case MidMsgType_DetectBoardExt:
		{
			TRACE("case MidMsgType_DetectBoardExt\n");
			if (mode == 3)
				ecode = 18;
			else
				detecthwext = 1;
			break;
		}
		case MidMsgType_GetBoardExt:
		{
			if (mode == 3)
				ecode = 18;
			else
			{
				uint8 devinfo[2];
				devinfo[0] = gBoardCompos.spi_ex;
				devinfo[1] = gBoardCompos.uart_ex;
				cus_ee_short_reply(rfd, buf, buf[4], 0, iscrc, sendfunc, crc_msg, prtset, devinfo, 2);
				return 0;
			}
			break;
		}
		case MidMsgType_SetGPO:
		{
			int gopos = 6;
			int gpocnt = pMsg[gopos++];
			unsigned char gpoid;
			unsigned char gpostate;
			int i;
			
			if (gEEcmdGpoSet.isFire == 1)
			{
				ecode = 16;
				break;
			}

			if (gpocnt == 0 && gpocnt > 5)
				ecode = 0x07;
			else
			{
				gEEcmdGpoSet.idcnt = gpocnt;
				for (i = 0; i < gpocnt; ++i)
				{
					gpoid = pMsg[gopos++];
					if (gpoid < 1 || gpoid > 5)
					{
						ecode = 0x07;
						break;
					}
					else
					{
						gpostate = pMsg[gopos++];
						if (gpostate > 1)
						{
							ecode = 0x07;
							break;			
						}
						else
						{
							gEEcmdGpoSet.ids[i] = gpoid;
							gEEcmdGpoSet.states[i] = gpostate;
							gEEcmdGpoSet.durs[i] = pMsg[gopos++]*1000;
						}
					}
				}
				if (nonrep_uint8_array(gEEcmdGpoSet.ids, gpocnt) != 1)
					ecode = 0x07;
			}
			
			if (ecode == 0)
			{
				gEEcmdGpoSet.isFire = 0;
				for (i = 0; i < gEEcmdGpoSet.idcnt; ++i)
				{
//					printf("id:%d,state:%d,dur:%d\n", gEEcmdGpoSet.ids[i], 
//						gEEcmdGpoSet.states[i], gEEcmdGpoSet.durs[i]);
					gpo_set(gEEcmdGpoSet.ids[i], gEEcmdGpoSet.states[i]);
					if (gEEcmdGpoSet.durs[i] == 0)
						gEEcmdGpoSet.finflags[i] = 1;
					else
					{
						gEEcmdGpoSet.finflags[i] = 0;
						gEEcmdGpoSet.isFire = 1;
					}
				}				
			}
			break;
		}
		case MidMsgType_Update_Fw_By_Ftp:
		{
			json_value* pobj;
			int ftpjlen = GetNumU16(pMsg+2);
			
			pMsg[6+ftpjlen] = 0;
			pobj = json_parse((char *)(pMsg+6), ftpjlen);
			if (pobj != NULL)
			{
				BtParams_ST *btparams = malloc_hexp(sizeof(BtParams_ST));
				btparams->updateflag = 'V';
				if (valid_upfw_ftp_params(pobj, btparams) == 0)
				{
					setBtParams(btparams);
					reboot = 1;
				}
				else
					ecode = 0x07;
				free_hexp(btparams);
			}
			else
				ecode = 0x07;
			break;
		}
		default:
			return -2;
	}
//	printf("before cus_ee_short_reply\n");
	cus_ee_short_reply(rfd, buf, buf[4], ecode, iscrc, sendfunc, crc_msg, prtset, NULL, 0);
//	printf("after cus_ee_short_reply\n");
	if (pData != NULL)
		free_hexp(pData);
	if (reboot == 1)
	{
//		printf("before system_reset\n");
		sleep_ms(400);
		system_reset();			
	}
	if (detecthwext == 1)
	{
		uart_close(COMMON_INTERFACE_UART1);
		Uart_Ex_Code uart1ex = detect_uart_ex_dev();
		Spi_Ex_Code spiex = detect_spi_ex_dev();
		TRACE("set_board_compos uart1ex:%d, spiex:%d\n", uart1ex, spiex);
		set_board_compos(spiex, uart1ex);
		sleep_ms(300);
		system_reset();
	}
	return 0;
}
#endif



