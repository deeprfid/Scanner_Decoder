#include "port.h"
#include "timer.h"
#include "wizchip_conf.h"
#include "hc32f46_driver.h"

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
    stcSpiInit.enClkDiv = SpiClkDiv4;  //64M/4=16M
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
// int i;
    M4_PORT->PODRB_f.POUT01 = 0;
    timer_Delay_ms(100);
    M4_PORT->PODRB_f.POUT01 = 1;
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


int w5100s_network_info_init(void)
{
    wiz_NetInfo info;
	 networkParaConfig netconf;
	 int ret;
	
    ret = get_network_config(&netconf);
	 if (ret != 0)
	 {
		 printf("get_network_config failed");
		 return -1;
	 }
    
    memcpy(info.mac, netconf.mac, 6);
    memcpy(info.ip, netconf.ip, 4);
    memcpy(info.sn, netconf.subnetMask, 4);
    memcpy(info.gw, netconf.gatewayIP, 4);
    memcpy(info.dns, netconf.dnsServer, 4);
    
#ifdef USE_DHCP
    info.dhcp = NETINFO_DHCP;
#else
    info.dhcp = NETINFO_STATIC;
#endif
    
    wizchip_setnetinfo(&info);
	 return 0;
}

void w5100s_network_info_show(void)
{
    wiz_NetInfo info;
    
    wizchip_getnetinfo(&info);
    
    printf("w5500 network infomation:\r\n");
    printf("  -mac:%d:%d:%d:%d:%d:%d\r\n", info.mac[0], info.mac[1], info.mac[2], 
            info.mac[3], info.mac[4], info.mac[5]);
    printf("  -ip:%d.%d.%d.%d\r\n", info.ip[0], info.ip[1], info.ip[2], info.ip[3]);
    printf("  -sn:%d.%d.%d.%d\r\n", info.sn[0], info.sn[1], info.sn[2], info.sn[3]);
    printf("  -gw:%d.%d.%d.%d\r\n", info.gw[0], info.gw[1], info.gw[2], info.gw[3]);
    printf("  -dns:%d.%d.%d.%d\r\n", info.dns[0], info.dns[1], info.dns[2], info.dns[3]);
    
    if (info.dhcp == NETINFO_DHCP) {
        printf("  -dhcp_mode: dhcp\r\n");
    } else {
        printf("  -dhcp_mode: static\r\n");
    }
}


commonSocketPara socketParams[_WIZCHIP_SOCK_NUM_];

int network_init(void)
{
	int i;
	
	
	WIZ_SPI_Init();
	Reset_W5100S();	

	reg_wizchip_cs_cbfunc(w5100s_cs_select, w5100s_cs_deselect);
	reg_wizchip_spi_cbfunc(w5100s_spi_readbyte, w5100s_spi_writebyte);
	
	if (wizchip_init(NULL, NULL) != 0)
	{
		printf("network_init error\n");
		return -1;
	}
	else
		printf("network_init success !\n");
	
	w5100s_network_info_init();
	w5100s_network_info_show();
	
	for (i = 0; i < _WIZCHIP_SOCK_NUM_; ++i)
	{
		socketParams[i].isBlock = O_BLOCK;
		close(i);
	}
	
	return 0;
}

