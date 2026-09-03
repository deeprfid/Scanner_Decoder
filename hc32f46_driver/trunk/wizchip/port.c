#include <stdlib.h>
#include "port.h"
#include "timer.h"
#include "wizchip_conf.h"
#include "hc32f46_driver.h"
#include "dhcp.h"
#include "driverconfig.h"

void WIZ_SPI_Init(void)
{
    stc_spi_init_t stcSpiInit;
    stc_port_init_t Port_CFG;
    MEM_ZERO_STRUCT(Port_CFG);
    MEM_ZERO_STRUCT(stcSpiInit);
    Port_CFG.enPinMode = Pin_Mode_Out;
    Port_CFG.enPinDrv = Pin_Drv_L;
    PORT_Init(PortA, Pin04, &Port_CFG);//SS
    M4_PORT->PODRA_f.POUT04 = 1;

    /* Configuration peripheral clock */
    PWC_Fcg1PeriphClockCmd(PWC_FCG1_PERIPH_SPI1, Enable);
    /* Configuration SPI pin */
    PORT_SetFunc(PortA, Pin05, Func_Spi1_Sck, Disable);
    PORT_SetFunc(PortA, Pin07, Func_Spi1_Mosi, Disable);
    PORT_SetFunc(PortA, Pin06, Func_Spi1_Miso, Disable);
    /* Configuration SPI structure */
    stcSpiInit.enClkDiv = SpiClkDiv2;  // 200M: PCLK1=100M -> 50MHz（用户实测 W5100S SPI 支持至 70M，Div2 稳定）
    stcSpiInit.enFrameNumber = SpiFrameNumber1;//1帧
    stcSpiInit.enDataLength = SpiDataLengthBit8;//8bits
    stcSpiInit.enFirstBitPosition = SpiFirstBitPositionMSB;//MSB
    stcSpiInit.enSckPolarity = SpiSckIdleLevelLow;//CPOL
    stcSpiInit.enSckPhase = SpiSckOddSampleEvenChange;
    stcSpiInit.enReadBufferObject = SpiReadReceiverBuffer;// 数据缓冲器读取对象
    stcSpiInit.enWorkMode = SpiWorkMode3Line;//3线模式
    stcSpiInit.enTransMode = SpiTransFullDuplex;//全双工
    stcSpiInit.enCommAutoSuspendEn = Disable;//自动挂起
    stcSpiInit.enModeFaultErrorDetectEn = Disable;//错误检测
    stcSpiInit.enParitySelfDetectEn = Disable;//奇偶校验
    stcSpiInit.enParityEn = Disable;   //增加奇偶校验位发送
    stcSpiInit.enParity = SpiParityEven;//偶校验

    stcSpiInit.enMasterSlaveMode = SpiModeMaster;
    stcSpiInit.stcDelayConfig.enSsSetupDelayOption = SpiSsSetupDelayCustomValue;
    stcSpiInit.stcDelayConfig.enSsSetupDelayTime = SpiSsSetupDelaySck1;
    stcSpiInit.stcDelayConfig.enSsHoldDelayOption = SpiSsHoldDelayCustomValue;
    stcSpiInit.stcDelayConfig.enSsHoldDelayTime = SpiSsHoldDelaySck1;
    stcSpiInit.stcDelayConfig.enSsIntervalTimeOption = SpiSsIntervalCustomValue;
    stcSpiInit.stcDelayConfig.enSsIntervalTime = SpiSsIntervalSck6PlusPck2;

    SPI_Init(M4_SPI1, &stcSpiInit);
    SPI_Cmd(M4_SPI1, Enable);
}

void WIZ_CS(uint8_t val)
{
    if (val == LOW)
    {
        PORT_ResetBits(PortA, Pin04);
    }
    else if (val == HIGH)
    {
        PORT_SetBits(PortA, Pin04);
    }
}

void w5100s_cs_select(void)
{
    PORT_ResetBits(PortA, Pin04);
}

void w5100s_cs_deselect(void)
{
    PORT_SetBits(PortA, Pin04);
}

void Reset_W5100S(void) // 2018-07-17
{
    M4_PORT->PODRB_f.POUT15 = 0;
    timer_Delay_ms(100);
    M4_PORT->PODRB_f.POUT15 = 1;
    timer_Delay_ms(1000);
}

uint8_t SPI1_SendByte(uint8_t byte)
{
    while (Reset == SPI_GetFlag(M4_SPI1, SpiFlagSendBufferEmpty)) {}
    SPI_SendData8(M4_SPI1, byte);
    while (Reset == SPI_GetFlag(M4_SPI1, SpiFlagReceiveBufferFull)) {}
    return (SPI_ReceiveData8(M4_SPI1)&0xff);
}

uint8_t w5100s_spi_readbyte(void)
{
		return SPI1_SendByte(0x00);
}
void w5100s_spi_writebyte(uint8_t wb)
{
		SPI1_SendByte(wb);
}

/* burst SPI (F4A0 parity): single-call bulk transfer, fewer per-byte indirections */
void SPI_WriteDatas(uint8_t *data, uint16_t len)
{
    while (len--)
        SPI1_SendByte(*data++);
}
void SPI_ReadDatas(uint8_t *data, uint16_t len)
{
    while (len--)
        *data++ = SPI1_SendByte(0x00);
}


void rdr_ip_conflict(void)
{
	TRACE("CONFLICT IP from DHCP\r\n");
   //halt or reset or any...
   system_reset();
}

wiz_NetInfo gNetInfo;
void rdr_ip_assign(void)
{
	TRACE("rdr_ip_assign -------------------------------------------\n");
   getIPfromDHCP(gNetInfo.ip);
   getGWfromDHCP(gNetInfo.gw);
   getSNfromDHCP(gNetInfo.sn);
   getDNSfromDHCP(gNetInfo.dns);
   /* Network initialization */
	gNetInfo.dhcp = NETINFO_DHCP;
	ctlnetwork(CN_SET_NETINFO, (void*)&gNetInfo);
	w5100s_network_info_show();
   TRACE("DHCP LEASED TIME : %d Sec.\r\n", getDHCPLeasetime());
}


extern const uint8 DefIpAddr[];
extern const uint8 DefSubnetMask[];
extern const uint8 DefGateWay[];
extern const uint8 DefDnsServer[];
extern const uint8 DefMac[];

void set_default_ip(void)
{
	memcpy(gNetInfo.ip, DefIpAddr, 4);
	memcpy(gNetInfo.sn, DefSubnetMask, 4);
	memcpy(gNetInfo.gw, DefGateWay, 4);
	memcpy(gNetInfo.dns, DefDnsServer, 4);
	/*
   getIPfromDHCP(netInfo.ip);
   getGWfromDHCP(netInfo.gw);
   getSNfromDHCP(netInfo.sn);
   getDNSfromDHCP(netInfo.dns);
	*/
   /* Network initialization */
	gNetInfo.dhcp = NETINFO_STATIC;
	ctlnetwork(CN_SET_NETINFO, (void*)&gNetInfo);
	w5100s_network_info_show();
}

int gIsConfDhcp = 0;
int gMaxAvailableSocket = _WIZCHIP_SOCK_NUM_;
networkParaConfig gNetConf;
int w5100s_network_info_init(int dhcpsn)
{
	 uint8 *dhcpbuf;
	
    gIsConfDhcp = is_netconf_dhcp(&gNetConf);
	 if (gIsConfDhcp < 0)
	 {
		 TRACE("get_network_config failed");
		 return -1;
	 }
    
	 memcpy(gNetInfo.mac, gNetConf.mac, 6);
	 memcpy(gNetInfo.dns, gNetConf.dnsServer, 4);
	 memcpy(gNetInfo.ip, gNetConf.ip, 4);
	 memcpy(gNetInfo.sn, gNetConf.subnetMask, 4);
	 memcpy(gNetInfo.gw, gNetConf.gatewayIP, 4);
	 
	 if (gIsConfDhcp == 1)
	 {
		 gNetInfo.dhcp = NETINFO_DHCP;
		 setSHAR(gNetInfo.mac);
		 TRACE("dhcp mac:%02X-%02X-%02X-%02X-%02X-%02X\n", gNetInfo.mac[0], 
			gNetInfo.mac[1], gNetInfo.mac[2], gNetInfo.mac[3], gNetInfo.mac[4],
			gNetInfo.mac[5]);
		 dhcpbuf = malloc_hexp(1024);
		 if (dhcpbuf == NULL)
		 {
			 TRACE("get_network_config dhcpbuf == NULL");
			 return -1;
		 }
		 if (dhcpsn == -1)
		 {
			 dhcpsn = getMaxSocketId();
		 }
		 DHCP_init(dhcpsn, dhcpbuf);
		 reg_dhcp_cbfunc(rdr_ip_assign, rdr_ip_conflict, rdr_ip_conflict);
	 }
	 else
	 {
		 gNetInfo.dhcp = NETINFO_STATIC;
		 wizchip_setnetinfo(&gNetInfo);
		 
	 }
    
	 return 0;
}

extern volatile int gIsDhcpIpSet;
void wait_fin_init(void)
{
	if (gIsConfDhcp == 1)
	{
		while(1)
		{
			sleep_ms(50);
			if (gIsDhcpIpSet == 1)
				break;
		}
	}
}



void w5100s_network_info_show(void)
{
    wiz_NetInfo info;
    
    wizchip_getnetinfo(&info);
    
    TRACE("w5500 network infomation:\r\n");
    TRACE("  -mac:%d:%d:%d:%d:%d:%d\r\n", info.mac[0], info.mac[1], info.mac[2], 
            info.mac[3], info.mac[4], info.mac[5]);
    TRACE("  -ip:%d.%d.%d.%d\r\n", info.ip[0], info.ip[1], info.ip[2], info.ip[3]);
    TRACE("  -sn:%d.%d.%d.%d\r\n", info.sn[0], info.sn[1], info.sn[2], info.sn[3]);
    TRACE("  -gw:%d.%d.%d.%d\r\n", info.gw[0], info.gw[1], info.gw[2], info.gw[3]);
    TRACE("  -dns:%d.%d.%d.%d\r\n", info.dns[0], info.dns[1], info.dns[2], info.dns[3]);
    
    if (info.dhcp == NETINFO_DHCP) {
        TRACE("  -dhcp_mode: dhcp\r\n");
    } else {
        TRACE("  -dhcp_mode: static\r\n");
    }
}

#if IS_RTOS2_SUPPORT
osMutexId_t mutex_cris_id;
osRtxMutex_t gWizportmux_cb;
void cris_mutex_en(void)
{
	osStatus_t  status;
   status  = osMutexAcquire(mutex_cris_id, osWaitForever);
   if (status != osOK)
		TRACE("osMutexAcquire error:%d\n", status);
}

void cris_mutex_ex(void)
{
	osStatus_t  status;
   status  = osMutexRelease(mutex_cris_id);
   if (status != osOK)
		TRACE("osMutexRelease error:%d\n", status);	
}
#endif

Spi_Ex_Code detect_spi_ex_dev(void)
{
   uint8_t mac[6] = {0};
	WIZ_SPI_Init();
	Reset_W5100S();
	reg_wizchip_cs_cbfunc(w5100s_cs_select, w5100s_cs_deselect);
	reg_wizchip_spi_cbfunc(w5100s_spi_readbyte, w5100s_spi_writebyte);
	reg_wizchip_spiburst_cbfunc(SPI_ReadDatas, SPI_WriteDatas);
   setSHAR((uint8_t *)DefMac);
   getSHAR(mac);
	if (memcmp(DefMac, mac, 6) == 0)
	{
		TRACE("find w5100s\n");
		return Spi_Ex_Ethernet;
	}
	else
	{
		TRACE("find no spi ex dev\n");
		return Spi_Ex_None;
	}
}

commonSocketPara gSocketParams[_WIZCHIP_SOCK_NUM_];

int network_init(int dhcpsn)
{
	int i;
#if IS_RTOS2_SUPPORT
	osMutexAttr_t Mutex_attr = {
	  "wizchip",
	  osMutexRecursive | osMutexPrioInherit,
	  &gWizportmux_cb,
	  sizeof(gWizportmux_cb)
	};
	mutex_cris_id = osMutexNew(&Mutex_attr);
	if (mutex_cris_id == NULL)
	{
		TRACE("network_init osMutexNew failed");
		return -1;
	}
#endif
	WIZ_SPI_Init();
	Reset_W5100S();
	reg_wizchip_cs_cbfunc(w5100s_cs_select, w5100s_cs_deselect);
	reg_wizchip_spi_cbfunc(w5100s_spi_readbyte, w5100s_spi_writebyte);
	reg_wizchip_spiburst_cbfunc(SPI_ReadDatas, SPI_WriteDatas);
	
#if IS_RTOS2_SUPPORT
	reg_wizchip_cris_cbfunc(cris_mutex_en, cris_mutex_ex);
#endif
	if (wizchip_init(NULL, NULL) != 0)
	{
		TRACE("network_init error\n");
		return -1;
	}
	else
		TRACE("network_init success !\n");
	
	w5100s_network_info_init(dhcpsn);
	w5100s_network_info_show();
	
	for (i = 0; i < _WIZCHIP_SOCK_NUM_; ++i)
	{
		gSocketParams[i].isBlock = O_BLOCK;
		gSocketParams[i].timeout = -1;
		close(i);
	}
	
	return 0;
}

extern volatile int gIsRunDnsTimeHandler;
void pre_DNS_init()
{
	gIsRunDnsTimeHandler = 1;
}
void aft_DNS_run()
{
	gIsRunDnsTimeHandler = 0;
}

void wiz_wait_link_on(void)
{
	uint8 tmp;
	int wcnt = 0;
	do{
			sleep_ms(100);
			if(ctlwizchip(CW_GET_PHYLINK, &tmp) == -1)
				printf("Unknown PHY Link stauts.\r\n");
			wcnt++;
			if (wcnt == 200)
				break;
	}while(tmp == PHY_LINK_OFF);
	sleep_ms(500);
}

