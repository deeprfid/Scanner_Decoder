#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "btMsg.h"
#include "hc32f46_driver.h"
#include "fw_jump_helper.h"
#include "update_by_ftp.h"
//#include "mp_pool.h"
#include "app_conf.h"
//0x1FFF8000
//0x2F000
/*
PendSV_Handler
SysTick_Handler
SVC_Handler
*/
//20026FF0

extern unsigned char  Image$$ER_IROM1$$Base;
extern const uint32_t u32ICG[];
extern uint32_t SystemCoreClock;
Spi_Ex_Code gSpiex = Spi_Ex_None;
Uart_Ex_Code gUartex = Uart_Ex_None;
	
int main(void)
{
	int i;
	int ret;
	int heap_base_address = ((int)&Image$$RW_IRAM1$$ZI$$Limit);
	BtMsgSt Msg;
	BtParams_ST btparams;
	networkParaConfig netset;
//	uint8 ipbins[4];
	commonUartPara uartPara;
	uint8 detectmod[] = {0xff, 0x00, 0x03, 0x1d, 0x0c};
	uint8 modverbuf[30];
	commonUartPara uart0Para;
	int Uarttimeout = 40;
	int Usbtimeout = 80;
	int m_rtm = 40;

	memset(&uartPara, 0, sizeof(commonUartPara));
	uartPara.isBlock	= O_BLOCK;
	uartPara.isPrintf	= 1;
	uartPara.baudrate	= 115200;
	uartPara.timeout	= 40;
	
	heap_base_address += 64 - heap_base_address %64;
	_init_alloc(heap_base_address, 0x20026FF0);

//	InitDegutPrintf(1, COMMON_INTERFACE_SOCKET2, "192.168.1.44", 9999);
	printf("u32ICG[0]:%d\n", u32ICG[0]);

	//////
#if JustJump2App
	run_app(0x16000);
	while(1);
#endif
	//////

	
#if (AppDubugPrintf == 1)
	RCC_Configuration();
	GPIO_Configuration();
   timer_Init();
	InitDegutPrintf(1, COMMON_INTERFACE_SOCKET2, "192.168.1.44", 9999);
#elif (AppDubugPrintf == 2)
	RCC_Configuration();
	GPIO_Configuration();
   timer_Init();
	network_init(COMMON_INTERFACE_SOCKET3);
	InitDegutPrintf(2, COMMON_INTERFACE_SOCKET2, "192.168.1.44", 9999);
#endif
	
	if (getBtParams(&btparams) < 0)
	{
		TRACE("verify params failed\n");
		goto loop;
	}
	else
	{
		dumpBtParams(&btparams);
		if (btparams.updateflag == 0)
		{
			if (verifyFirmware(btparams.firmwareaddr, 
				btparams.firmwaresize, btparams.firmwarecrc) ==  0)
			{
				TRACE("ready boot firmware\n");
				run_app(btparams.firmwareaddr);
			}
			else
			{
				TRACE("Firmware verification failed\n");
				goto loop;
			}
		}
		else if (btparams.updateflag == 1)
		{
			TRACE("firmware update did not finish\n");
			goto loop;
		}
		else
		{
			TRACE("waiting bootloader commands\n");
			goto loop;
		}
	}
	
loop:
	TRACE("enter loop \n");
#if (AppDubugPrintf == 0)
	RCC_Configuration();
	GPIO_Configuration();
   timer_Init();
#endif
	init_usb(1);
	sleep_ms(100);
	
	gSocks[0] = COMMON_INTERFACE_SOCKET0;
	gSocks[1] = COMMON_INTERFACE_SOCKET1;
	gStatusFlags[0] = 0;
	gStatusFlags[1] = 0;
	get_network_config(&netset);
	gTcpport = 8080;
#if (AppDubugPrintf != 1)
	uart_open(COMMON_INTERFACE_UART2, &uartPara);
	ioctl(COMMON_INTERFACE_UART2, COMMON_INTERFACE_SET_TIMEOUT, &Uarttimeout);
#endif
	
	uartPara.t485 = 1;
	uart_open(COMMON_INTERFACE_UART3, &uartPara);
	ioctl(COMMON_INTERFACE_UART3, COMMON_INTERFACE_SET_TIMEOUT, &Uarttimeout);
	
	ioctl(COMMON_INTERFACE_USB0, COMMON_INTERFACE_SET_TIMEOUT, &Usbtimeout);
	ioctl(COMMON_INTERFACE_SOCKET0, COMMON_INTERFACE_SET_TIMEOUT, &m_rtm);
	ioctl(COMMON_INTERFACE_SOCKET1, COMMON_INTERFACE_SET_TIMEOUT, &m_rtm);
	/////////
	if (get_board_compos() != 0)
	{
		TRACE("if (get_board_compos() != 0)\n");
		gSpiex = detect_spi_ex_dev();
	}
	else
	{
		gSpiex = get_spi_ex_dev();
		gUartex = get_uart_ex_dev();		
	}
	TRACE("get_board_compos ok, uart1ex:%d, spiex:%d\n", gUartex, gSpiex);
	
#if WlanBlueUpdateSup	
	if (gUartex == Uart_Ex_Wlan)
		init_wlan(115200, NULL, 8080, 0, 0);
	else if (gUartex == Uart_Ex_Bluetooth)
		init_bluetooth(115200);
#endif
	///////////
#if (AppDubugPrintf != 2)
	if (gSpiex == Spi_Ex_Ethernet)
	{
		network_init(COMMON_INTERFACE_SOCKET3);
		brdcst_conf_init(COMMON_INTERFACE_SOCKET2);
		dump_network_config(&gNetConf);
	}
#endif
	
	memset(&uart0Para, 0, sizeof(commonUartPara));
	uart0Para.isBlock	= O_BLOCK;
	uart0Para.isPrintf	= 0;
	uart0Para.baudrate	= 115200;
	uart0Para.timeout	= 1000;
	uart_open(COMMON_INTERFACE_UART0, &uart0Para);
	write(COMMON_INTERFACE_UART0, detectmod, sizeof(detectmod));
	if (read_n(COMMON_INTERFACE_UART0, modverbuf, 7) == 7)
	{
		if (read_n(COMMON_INTERFACE_UART0, modverbuf+7, modverbuf[1]) == modverbuf[1])
		{
			TRACE("find module\n");
			memcpy(gBrdCstDevInfo.modtype, modverbuf+9, 2);
			memcpy(gBrdCstDevInfo.modfwver, modverbuf+13, 4);
		}
	}

	/////////
if (gSpiex == Spi_Ex_Ethernet)
{
	if (gIsConfDhcp == 1)
		dhcp_wait();
}

#if EthernetFTPSup
	if ((btparams.updatemode == FwUpdateMode_ByFtp_FmEth && gSpiex == Spi_Ex_Ethernet) || 
		(btparams.updatemode == FwUpdateMode_ByFtp_Fm4G && gUartex == Uart_Ex_4G))
	{
		char *fppos;
		uint16 fport;
		fppos = strstr(btparams.ftpaddr, ":");
		if (fppos == NULL)
			fport = 21;
		else
		{
			fppos[0] = 0;
			fport = atoi_Arm(fppos+1);
		}
		TRACE("server:%s, port:%d\n", btparams.ftpaddr, fport);
		rfid_power_off();
		sleep_ms(1500);
		rfid_power_on();
		sleep_ms(1500);
		
		if (btparams.updatemode == FwUpdateMode_ByFtp_FmEth)
		{
			uint8 ftpservip[4];
			uint8 *helpbuf;
			uint64 starttime;
			uint64 nowtime;
			
			helpbuf = malloc(2048);
			if (IsIpv4address(btparams.ftpaddr) != 1)
			{
				wiz_NetInfo info;
				pre_DNS_init();
				DNS_init(COMMON_INTERFACE_SOCKET3, helpbuf);
				
				wizchip_getnetinfo(&info);			
				TRACE("dnsServer:%d.%d.%d.%d\n", info.dns[0], 
					info.dns[1], info.dns[2], info.dns[3]);
				
				ret =  DNS_run(info.dns, (uint8 *) btparams.ftpaddr, ftpservip);
				if (ret != 1)
				{
					TRACE("DNS_run error\n");
					sleep_ms(1000*120);
					system_reset();
				}
				else
				{
					TRACE("DNS_run success: %d.%d.%d.%d\n", ftpservip[0], ftpservip[1],
						ftpservip[2], ftpservip[3]);
				}
				aft_DNS_run();
			}
			else
			{
				if (addr_str2bin(btparams.ftpaddr, ftpservip) != 0)
				{
					led_toggle(-1, 120, brdcst_conf_handler);
					while(1);
				}
			}

			close(COMMON_INTERFACE_SOCKET0);
			close(COMMON_INTERFACE_SOCKET1);
			ftpc_init(ftpservip, btparams.ftpuser, btparams.ftppassword, 
				btparams.filename, fport, ftp_init_callback, ftp_data_callback);

			starttime = getSysTick();
			while(1)
			{
				brdcst_conf_handler();
				ret = ftpc_run(helpbuf);
				TRACE("ftpc_run ret:%d\n", ret);
				if (ret == 226)
				{
					ftpc_destory();
					aft_ftp_update_fw(&btparams);
				}
				else if (ret == -12345) //network error
				{
					ftpc_destory();
					led_toggle(180*1000, 700, brdcst_conf_handler);			
					system_reset();
				}
				else if (ret == -12346) //user or pwd error
				{
					ftpc_destory();
					led_toggle(-1, 80, brdcst_conf_handler);
				}
				else if (ret == -12347) //file not find
				{
					TRACE("befroe ftpc_destory\n");
					ftpc_destory();
					TRACE("after ftpc_destory\n");
					led_toggle(360*1000, 300, brdcst_conf_handler);				
					system_reset();
				}
				else
				{
					nowtime = getSysTick();
					if (nowtime - starttime > 140*1000)
					{
						TRACE("ftp init start failed \n");
						ftpc_destory();
						TRACE("-----------------\n");
						sleep_ms(1000*120);			
						system_reset();				
					}
				}
				sleep_ms(100);
			}			
		}
		else if (btparams.updatemode == FwUpdateMode_ByFtp_Fm4G)
		{
			init_4g_noconf(115200, 15000);
	//		usr_ftp_update_fw("wh-nbe33cl16g85ceddlwb.my3w.com", 21, "wh-nbe33cl16g85ceddlwb",
	//			"Silion123456", "myfolder/bsp_hc32f460_uart.h");
			/*
			if (usr_ftp_update_fw("43.138.155.217", 21, "RfidDemo",
				"Silion123", "std_hc32only_app_30.0.0.0_8.slfw") == 0)
				aft_ftp_update_fw(&btparams);
			else
			{
				sleep_ms(20000);
				system_reset();
			}
			*/
			if (usr_ftp_update_fw(btparams.ftpaddr, fport, btparams.ftpuser,
				btparams.ftppassword, btparams.filename) == 0)
				aft_ftp_update_fw(&btparams);
			else
			{
				sleep_ms(20000);
				system_reset();
			}
		}
	}
	else
	{
#endif
//		printf("before while (1)\n");
		while (1)
		{
			if (RecvMsg(&Msg) < 0)
			{
				continue;
			}
			else
			{
				Msg.StatusCode[0] = 0;
				Msg.StatusCode[1] = 0;
				
				switch(Msg.MsgCode)
				{
					case Soft_Version:
					{
//						printf("Msg.MsgCode == Soft_Version\n");
						if (getBtParams(&btparams) < 0)
							btparams.firmwarever=0;
						
						Msg.Datalen = 8;				
						Msg.data[0] = (btparams.firmwarever >> 24) & 0xff;
						Msg.data[1] = (btparams.firmwarever >> 16) & 0xff;
						Msg.data[2] = (btparams.firmwarever >> 8) & 0xff;
						Msg.data[3] = (btparams.firmwarever >> 0) & 0xff;

						Msg.data[4] = armBootVersion[0];
						Msg.data[5] = armBootVersion[1];
						Msg.data[6] = armBootVersion[2];
						Msg.data[7] = armBootVersion[3];
						break;
					}
					case Module_PowerOn:
					{
//						printf("Msg.MsgCode == Module_PowerOn\n");
						rfid_power_on();
						Msg.Datalen = 0;
						break;
					}
					case Module_PowerOff:
					{
//						printf("Msg.MsgCode == Module_PowerOff\n");
						rfid_power_off();
						Msg.Datalen = 0;
						break;
					}
					case Boot_Firmware:
					{
//						printf("Msg.MsgCode == Boot_Firmware\n");
						Msg.Datalen = 0;
						if (getBtParams(&btparams) < 0)
						{
							Msg.StatusCode[0] = 0x90;
							Msg.StatusCode[1] = 0x18;
						}
						else
						{
							if (btparams.updateflag != 0)
							{
								btparams.updateflag = 0;
								setBtParams(&btparams);
							}
							SendRespMsg(&Msg);
							sleep_ms(200);
							system_reset();
						}
						break;
					}
					case HC32F46X_SEND_DATA_INPAGE:
					{
						int offset = (Msg.data[2] << 8) | Msg.data[3];
						int nbytes = (Msg.Datalen - 4);
//						printf("Msg.MsgCode == HC32F46X_SEND_DATA_INPAGE offset:%d, nbytes:%d\n", offset, nbytes);
						unsigned char *pwdata = Msg.data+4;
						memcpy(gPageBuffer+offset, pwdata, nbytes);
						Msg.Datalen = 0;
						break;
					}
					case HC32F46X_WRITE_PAGE:
					{
						int pagenum = (Msg.data[0] << 8) | Msg.data[1];
//						printf("Msg.MsgCode == HC32F46X_WRITE_PAGE pagenum:%d\n", pagenum);
						fw_revert4bytes(gPageBuffer, PAGE_SIZE);
						flash_sector_erase(pagenum*PAGE_SIZE);
						flash_bytes_write(pagenum*PAGE_SIZE, gPageBuffer, PAGE_SIZE);
						Msg.Datalen = 0;
						break;
					}
					case HC32F46X_VERIFY_FIRMWARE:
					{
						uint32 fwaddr = GetNumU32(Msg.data);
						uint32 fwlen = GetNumU32(Msg.data+4);
						uint32 fwcrc = GetNumU32(Msg.data+8);
//						printf("Msg.MsgCode == HC32F46X_VERIFY_FIRMWARE fwaddr:%d, fwlen:%d, fwcrc:%d\n", 
//							fwaddr, fwlen, fwcrc);
						Msg.Datalen = 0;
						if (verifyFirmware(fwaddr, fwlen, fwcrc) != 0)
						{
								Msg.StatusCode[0] = 0x90;
								Msg.StatusCode[1] = 0x14;
						}
						break;
					}
					case HC32F46X_WRITE_BTPARAMS:
					{
						btparams.updateflag = GetNumU32(Msg.data);
						btparams.firmwareaddr = GetNumU32(Msg.data+4);
						btparams.firmwaresize = GetNumU32(Msg.data+8);
						btparams.firmwarecrc = GetNumU32(Msg.data+12);
						btparams.firmwarever = GetNumU32(Msg.data+16);
						
						btparams.updatemode = FwUpdateMode_Default;
						btparams.ftpuser[0] = 0;
						btparams.ftppassword[0] = 0;
						btparams.ftpaddr[0] = 0;
						btparams.filename[0] = 0;
//						printf("Msg.MsgCode == HC32F46X_WRITE_BTPARAMS\n");
						dumpBtParams(&btparams);
						Msg.Datalen = 0;
						if (setBtParams(&btparams)	!= 0 )
						{
							Msg.StatusCode[0] = 0x90;
							Msg.StatusCode[1] = 0x14;							
						}
						break;
					}
					case Set_Relay_Com:
					{
						int cmdnum = Msg.data[0];
						uint32 bps = (Msg.data[1] << 24) | (Msg.data[2] << 16) | (Msg.data[3] << 8) | 
							(Msg.data[4] << 0);
//						printf("Msg.MsgCode == Set_Relay_Com bps: %d\n", bps);
						Msg.Datalen = 0;
						if (cmdnum == 1)
						{
							uart_close(COMMON_INTERFACE_UART0);
							commonUartPara uart0Para;
							uart0Para.isBlock	= O_BLOCK;
							uart0Para.isPrintf	= 0;
							uart0Para.baudrate	= bps;
							uart0Para.timeout	= 3000;

							if (uart_open(COMMON_INTERFACE_UART0, &uart0Para) < 0)
							{
								Msg.StatusCode[0] = 0x90;
								Msg.StatusCode[1] = 0x16;	
							}								
						}
						else
						{
							Msg.StatusCode[0] = 0x90;
							Msg.StatusCode[1] = 0x16;	
						}
						break;
					}
					case Relay_Cmd:
					{
						/*
						int i;
						printf("Msg.MsgCode == Relay_Cmd\n");
						printf("relay cmd dump start\n");
							for (i = 0; i < Msg.Datalen; ++i)
								printf(" %02X", Msg.data[i]);
						printf("\n relay cmd dump end\n");
						*/
						
						if (write(COMMON_INTERFACE_UART0, Msg.data+4, Msg.Datalen-4) != Msg.Datalen-4)
						{
							Msg.StatusCode[0] = 0x90;
							Msg.StatusCode[1] = 0x17;
							Msg.Datalen = 0;
						}
						else
						{
							int exetm = (Msg.data[0] << 8) | Msg.data[1];
//							int bytetm = (Msg.data[2] << 8) | Msg.data[3];
							int blkmode;
//							printf("exetm:%d, bytetm:%d\n", exetm, bytetm);

							blkmode = O_BLOCK;
							ioctl(COMMON_INTERFACE_UART0, COMMON_INTERFACE_SET_ISBLOCK, &blkmode);
							ioctl(COMMON_INTERFACE_UART0, COMMON_INTERFACE_SET_TIMEOUT, &exetm);
							read(COMMON_INTERFACE_UART0, Msg.data, 1);
							sleep_ms(50);
							blkmode = O_NONBLOCK;
							ioctl(COMMON_INTERFACE_UART0, COMMON_INTERFACE_SET_ISBLOCK, &blkmode);
							Msg.Datalen = read(COMMON_INTERFACE_UART0, Msg.data+1, 256);
							if (Msg.Datalen == 0)
							{
								TRACE("read error :%d\n", Msg.Datalen);
								Msg.StatusCode[0] = 0x90;
								Msg.StatusCode[1] = 0x17;
								Msg.Datalen = 0;
							}
							else
								Msg.Datalen++;
							/*
							printf("responds dump start\n");
							for (i = 0; i < Msg.Datalen; ++i)
								printf(" %02X", Msg.data[i]);
							printf("\n responds dump end\n");
							*/
							if (Msg.Datalen > 30)
								while(1);
						}
						break;
					}
					default:
					{
	//					printf("Msg.MsgCode Err\n");
						Msg.StatusCode[0] = 0x90;
						Msg.StatusCode[1] = 0x00;
						Msg.Datalen = 0;	
						break;
					}
				}
				SendRespMsg(&Msg); 
			}
		}
#if EthernetFTPSup		
	}
#endif
	return 0;
}


