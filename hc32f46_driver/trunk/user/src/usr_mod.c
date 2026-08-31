#include <string.h>
#include <stdlib.h>
#include "hc32f46_driver.h"
#include "common.h"
#include "driverconfig.h"

char gUsrCmdPwd[20];
int ATSendAndRecv(int uart1, char *buffer, int blen, char *atcmd, int wtm)
{
	int ret;
	int err = 0;
	int timeout;
	if (atcmd != NULL)
	{
		strcpy(buffer, atcmd);
		TRACE("time:%lld, send:%s\n", getSysTick(),atcmd);
	}
	else
		TRACE("time:%lld, send:%s\n", getSysTick(),buffer);
	
	strcat(buffer, "\r");
	
	write(uart1, buffer, strlen(buffer));
	if (wtm == -1)
		timeout = 2000;
	else
		timeout = wtm;
	ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_SET_TIMEOUT, &timeout);
	ret = read(uart1, buffer, 1);
	if (ret > 0)
	{
		sleep_ms(30);
		ret = read(uart1, buffer+1, blen-2);
		if (ret < 3)
		{
			TRACE("ret:%d\n", ret);
			if (ret > 0)
			{
				buffer[ret+1] = 0;
				TRACE("err resp:%s\n", buffer);
			}
			err = -1;
			goto FIN;			
		}
		else
		{
			buffer[ret+1] = 0;
			TRACE("recv:%s\n", buffer);
			if (strstr(buffer, "OK") != NULL || strstr(buffer, "ok") != NULL || 
				strstr(buffer, "Ok") != NULL || strstr(buffer, "oK") != NULL)
				err = 0;
			else
			{
				TRACE("resp not find ok ret:%d\n", ret);
				err = -1;
			}
			goto FIN;			
		}
	}
	else
	{
		err = -1;
		goto FIN;
	}
	
FIN:
	ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
	return err;
}

int enter_config_mode(int baud, int isrepw)
{
	int i;
	char buffer[30];
	int ret;

	commonUartPara uart1Para;
	memset(&uart1Para, 0, sizeof(commonUartPara));
	uart1Para.isBlock	= O_BLOCK;
	uart1Para.isPrintf	= 0;
	uart1Para.baudrate	= baud;
	uart1Para.timeout	= 1000;
	uart1Para.isRdam = 1;
	
	uart_open(COMMON_INTERFACE_UART1, &uart1Para);

	if (isrepw == 1)
	{
		ex_power_off();
		sleep_ms(1000);
		ex_power_on();
	}
	TRACE("try %d send +++\n", baud);
	for (i = 0; i < 11; ++i)
	{
		strcpy(buffer, "+++");
		write(COMMON_INTERFACE_UART1, buffer, 3);
		ret = read(COMMON_INTERFACE_UART1, buffer, 1);
		if (ret > 0)
		{
			if (buffer[0] == 'a')
			{
				TRACE("send a\n");
				write(COMMON_INTERFACE_UART1, buffer, 1);
				ret = read_n(COMMON_INTERFACE_UART1, buffer, 3);
				buffer[3] = 0;
				TRACE("%s\n", buffer);
				TRACE("enter_config_mode ok i:%d\n", i);
				return 0;
			}
			else
			{
				sleep_ms(50);
				ret = read(COMMON_INTERFACE_UART1, buffer+1, 28);
				buffer[1+ret] = 0;
				TRACE("uart1 output:%s\n", buffer);
				ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
			}
		}
	}
	
	return -1;
}

int find_uar1_ex_mod(int *bauds, int bcnt)
{
	int i;
	
	for (i = 0; i < bcnt; ++i)
	{
		if (enter_config_mode(bauds[i], 1) == 0)
			return 0;
		else
			uart_close(COMMON_INTERFACE_UART1);
	}
	return -1;
}
/*
int find_uar1_ex_mod(int baud1, int baud2, int is576)
{
	int err;
	err = enter_config_mode(baud1, 1);
	if (err != 0)
	{
		uart_close(COMMON_INTERFACE_UART1);
		err = enter_config_mode(baud2, 1);
		
		if (is576 == 1 && err != 0)
		{
			uart_close(COMMON_INTERFACE_UART1);
			err = enter_config_mode(57600, 1);
		}
	}
	if (err != 0)
		return -1;

	return 0;
}
*/
Uart_Ex_Code detect_uart_ex_dev()
{
	int err;
	char buffer[50];
	char cmd[40];
	int bauds[4];
	
	bauds[0] = 115200;
	bauds[1] = 460800;
	bauds[2] = 57600;
	bauds[3] = 38400;
	
	err = find_uar1_ex_mod(bauds, 4);
	if (err == 0)
	{
		if (ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, "AT+WMODE", -1) == 0)
		{
			if (strstr(buffer, "AP") != NULL || strstr(buffer, "STA") != NULL || 
				strstr(buffer, "APSTA") != NULL)
			{				
				sprintf(cmd, "AT+UART=%d,8,1,NONE,NFC", 115200);
				ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, cmd, -1);
				return Uart_Ex_Wlan;
			}
			else
				return Uart_Ex_Bluetooth;
		}
		else
		{
			sleep_ms(100);
			ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
			if (ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, "AT+WKMOD", -1) == 0)
			{
				sprintf(cmd, "AT+UART=%d,8,1,NONE,NONE", 115200);
				ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, cmd, -1);
				return Uart_Ex_4G;
			}
			else
				return Uart_Ex_None;
		}
	}
	else
		return Uart_Ex_None;
}

#define IF_ATSend(rbuf, rblen, cbuf, atcmd, wtm, isup) \
	do { \
		E(ATSendAndRecv(COMMON_INTERFACE_UART1, rbuf, rblen, atcmd, -1)); \
		if (isup == 1) \
			toupper_arm(rbuf); \
		if (strstr(buffer, cbuf) == NULL) \
		{ \
			sprintf(buffer, "%s=%s", atcmd, cbuf); \
			E(ATSendAndRecv(COMMON_INTERFACE_UART1, rbuf, rblen, NULL, wtm)); \
			chanset = 1; \
		} \
	} while (0)
	
	
int get_wlan_wslk(void)
{
	char buffer[50];
	int err;
	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, "AT+WSLK", 10000)); 
FIN:
	return err;
}

uint8 gBleMac[6];
int init_bluetooth(int baud)
{
	int err;
	char *buffer;
	char *cmpbuf = NULL;
	int rbuflen = 200;
	int chanset = 0;
	char *tmp1;
	BluetoothConfig_ST *blest;
	int bauds[2];
	
	bauds[0] = baud;
	bauds[1] = (baud == 115200?57600:115200);
	
	if (find_uar1_ex_mod(bauds, 2) != 0)
		return -1;

	TRACE("000 after enter_config_mode ret:%d\n", err);
	
	cmpbuf = malloc_hexp(100);
	buffer = malloc_hexp(rbuflen);
	blest = malloc_hexp(sizeof(BluetoothConfig_ST));
	
	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+MAC", -1));
	tmp1 = strstr(buffer, ":");
	strTohex(tmp1+1, 12, gBleMac);
	
	sprintf(cmpbuf, "%d,8,0,0", baud);
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+UART", -1, 1);
	
	sprintf(cmpbuf, "Slave");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+MODE", -1, 0);
	
	sprintf(cmpbuf, "0");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+WMODE", -1, 0);
	
	get_bluetooth_config(blest);
	
	sprintf(cmpbuf, "%s", blest->name);
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+NAME", -1, 0);
	
	if (blest->std_ble_pair == 0)
		sprintf(cmpbuf, "OFF");
	else
		sprintf(cmpbuf, "ON");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+PAIR", -1, 1);
	
	if (blest->pwd_pair == 0)
		sprintf(cmpbuf, "OFF");
	else
		sprintf(cmpbuf, "ON");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+PASSEN", -1, 1);
	
	if (blest->pwd_pair == 1)
	{
		sprintf(cmpbuf, "%s", blest->pwd);
		IF_ATSend(buffer, rbuflen, cmpbuf, "AT+PASS", -1, 1);
	}
	
	if (chanset == 1)
	{
		E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+Z", -1));
		sleep_ms(500);
		uart_close(COMMON_INTERFACE_UART1);
		E(enter_config_mode(115200, 0));
	}
	
	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+ENTM", -1));
	
FIN:
	free_hexp(buffer);
	free_hexp(cmpbuf);
	free_hexp(blest);
	return err;
}

void set_wlan4g_bauds(int baud, int *bauds)
{
	bauds[0] = baud;
	if (baud == 38400)
	{
		bauds[1] = 115200;
		bauds[2] = 460800;
	}
	else
	{
		bauds[1] = (baud == 115200?460800:115200);
		bauds[2] = 38400;
	}
}
int check_wlan(int baud, char *ssid, char *pwd)
{
	char buffer[50];
	int i;
	int err;
	char cmdbuf[50];
	int bauds[3];
	
	set_wlan4g_bauds(baud, bauds);
	
	E(find_uar1_ex_mod(bauds, 3));
	
	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, "AT+WMODE=STA", -1));
	sprintf(cmdbuf, "AT+WSTA=%s,%s", ssid, pwd);
	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, cmdbuf, -1));
	sprintf(cmdbuf, "AT+WANN=DHCP");
	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, cmdbuf, 5000));
	
	for (i = 0; i < 15; ++i)
	{
		sleep_ms(1500);
		E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, "AT+WANN", -1));
		if (strstr(buffer, "0.0.0.0") == NULL)
			return 0;
	}

	err = -1;
FIN:
	return err;
}

networkParaConfig gWlanNet;
int init_wlan(int baud, char *domain, uint16 serport, int waitconn, int waittm)
{
	int err;
	char *buffer;
	char *cmpbuf = NULL;
	char *tmp1;
	int timeout;
	int rbuflen = 120;
	int chanset = 0;
	Wlan_WMode_Code wmode_now;
	WlanConfig_ST *wlanst = NULL;
	int bauds[3];
	
	set_wlan4g_bauds(baud, bauds);
	
	E(find_uar1_ex_mod(bauds, 3));
	TRACE("000 after enter_config_mode ret:%d\n", err);
	
	timeout = 2000;
	ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_SET_TIMEOUT, &timeout);
	
	wlanst = malloc_hexp(sizeof(WlanConfig_ST));
	cmpbuf = malloc_hexp(100);
	buffer = malloc_hexp(rbuflen);
	
	sprintf(cmpbuf, "OFF");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+E", -1, 1);
	
	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+WMODE", -1));
	if (strncmp(buffer, "+ok=APSTA", 9) == 0)
		wmode_now = Wlan_WMode_AP_STA;
	else if (strncmp(buffer, "+ok=AP", 6) == 0)
		wmode_now = Wlan_WMode_AP;
	else if (strncmp(buffer, "+ok=STA", 7) == 0)
		wmode_now = Wlan_WMode_STA;

	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+WSMAC", -1));
	
	tmp1 = strstr(buffer, "=");
	if (tmp1 == NULL)
	{
		err = -1;
		goto FIN;
	}
	E(strTohex(tmp1+1, 12, gWlanNet.mac));
	//////
//	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+MDCH", -1));
	//////
	sprintf(cmpbuf, "%d,8,1,NONE,NFC", baud);
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+UART", -1, 1);

	if (get_wlan_config(wlanst) == 0)
	{
		dump_wlan_config(wlanst);
		if (wlanst->mode == Wlan_WMode_AP)
		{
			TRACE("get_wlan_config ok Wlan_WMode_AP\n");
			if (wlanst->mode != wmode_now)
			{
				E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+WMODE=AP", -1));
				chanset = 1;
			}
			
			sprintf(cmpbuf, "11BGN,%s,CH6", wlanst->ssid);
			IF_ATSend(buffer, rbuflen, cmpbuf, "AT+WAP", -1, 0);
			
			if (wlanst->pwd[0] != 0)
				sprintf(cmpbuf, "WPA2PSK,AES,%s", wlanst->pwd);
			else
				sprintf(cmpbuf, "OPEN,NONE,NONE");
			IF_ATSend(buffer, rbuflen, cmpbuf, "AT+WAKEY", -1, 0);
			
			sprintf(cmpbuf, "%d.%d.%d.%d,%d.%d.%d.%d", wlanst->ipinfo.ip[0], wlanst->ipinfo.ip[1], 
				wlanst->ipinfo.ip[2], wlanst->ipinfo.ip[3], wlanst->ipinfo.subnetMask[0], 
				wlanst->ipinfo.subnetMask[1], wlanst->ipinfo.subnetMask[2], wlanst->ipinfo.subnetMask[3]);
			IF_ATSend(buffer, rbuflen, cmpbuf, "AT+LANN", 5000, 1);
		}
		else if (wlanst->mode == Wlan_WMode_STA)
		{
			TRACE("get_wlan_config ok Wlan_WMode_STA\n");
			if (wlanst->mode != wmode_now)
			{
				E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+WMODE=STA", -1));
				chanset = 1;
			}
			
			sprintf(cmpbuf, "%s,%s", wlanst->ssid, wlanst->pwd);
			IF_ATSend(buffer, rbuflen, cmpbuf, "AT+WSTA", 5000, 0);

			if (wlanst->ipinfo.ip[0] == 0 && wlanst->ipinfo.ip[1] == 0 && 
				wlanst->ipinfo.ip[2] == 0 && wlanst->ipinfo.ip[3] == 0)
				sprintf(cmpbuf, "DHCP");
			else
			{
				sprintf(cmpbuf, "%d.%d.%d.%d", wlanst->ipinfo.dnsServer[0], wlanst->ipinfo.dnsServer[1], 
					wlanst->ipinfo.dnsServer[2], wlanst->ipinfo.dnsServer[3]);
				IF_ATSend(buffer, rbuflen, cmpbuf, "AT+WSDNS", 5000, 0);
				
				sprintf(cmpbuf, "STATIC,%d.%d.%d.%d,%d.%d.%d.%d,%d.%d.%d.%d", wlanst->ipinfo.ip[0], 
					wlanst->ipinfo.ip[1], wlanst->ipinfo.ip[2], wlanst->ipinfo.ip[3], wlanst->ipinfo.subnetMask[0], 
					wlanst->ipinfo.subnetMask[1], wlanst->ipinfo.subnetMask[2], wlanst->ipinfo.subnetMask[3], 
					wlanst->ipinfo.gatewayIP[0], wlanst->ipinfo.gatewayIP[1], wlanst->ipinfo.gatewayIP[2], 
					wlanst->ipinfo.gatewayIP[3]);
			}
			IF_ATSend(buffer, rbuflen, cmpbuf, "AT+WANN", 5000, 1);
		}
	}
	else
	{
		TRACE("get_wlan_config(wlanst) != 0\n");
		if (wmode_now != Wlan_WMode_AP)
		{
			E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+WMODE=AP", -1));
			chanset = 1;
		}
		setdef_wlan_config(wlanst);
		sprintf(cmpbuf, "11BGN,UHF-Reader,CH6");
		IF_ATSend(buffer, rbuflen, cmpbuf, "AT+WAP", -1, 0);
		
		sprintf(cmpbuf, "OPEN,NONE,NONE");
		IF_ATSend(buffer, rbuflen, cmpbuf, "AT+WAKEY", -1, 1);
		
		sprintf(cmpbuf, "NONE");
		IF_ATSend(buffer, rbuflen, cmpbuf, "AT+SOCKB", -1, 1);
		
		sprintf(cmpbuf, "%d.%d.%d.%d,%d.%d.%d.%d", wlanst->ipinfo.ip[0], 
			wlanst->ipinfo.ip[1], wlanst->ipinfo.ip[2], wlanst->ipinfo.ip[3], 
			wlanst->ipinfo.subnetMask[0], wlanst->ipinfo.subnetMask[1], 
			wlanst->ipinfo.subnetMask[2], wlanst->ipinfo.subnetMask[3]);
		
		IF_ATSend(buffer, rbuflen, cmpbuf, "AT+LANN", 5000, 1);
	}
	memcpy(&gWlanNet, &wlanst->ipinfo, sizeof(gWlanNet));
	
	if (domain != NULL)
		sprintf(cmpbuf, "TCP,CLIENT,%d,%s", serport, domain);
	else
	{
		if (serport == 0)
			sprintf(cmpbuf, "TCP,SERVER,%d,%d.%d.%d.%d",wlanst->ipinfo.listenPort,
				wlanst->ipinfo.ip[0], wlanst->ipinfo.ip[1], wlanst->ipinfo.ip[2], 
				wlanst->ipinfo.ip[3]);
		else
			sprintf(cmpbuf, "TCP,SERVER,%d,%d.%d.%d.%d",serport,
				wlanst->ipinfo.ip[0], wlanst->ipinfo.ip[1], wlanst->ipinfo.ip[2], 
				wlanst->ipinfo.ip[3]);			
	}
	
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+NETP", 5000, 0);
			
	sprintf(cmpbuf, "THROUGHPUT");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+TMODE", 5000, 1);
	
	if (chanset == 1)
	{
		E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+Z", -1));
		sleep_ms(500);
		uart_close(COMMON_INTERFACE_UART1);
		E(enter_config_mode(baud, 0));
	}
	
	if (waitconn == 1)
	{
		uint64 start = getSysTick();
		while(1)
		{
			E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+TCPLK", -1));
			toupper_arm(buffer);
			if (strstr(buffer, "+OK=ON") != NULL)
			{
				TRACE("connect server ok!!!!!\n");
				break;
			}
			else
			{
				if (getSysTick() - start >= waittm)
				{
					err = -1;
					goto FIN;
				}					
				sleep_ms(200);				
			}
		}
	}
	
	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+ENTM", -1));

FIN:
	free_hexp(buffer);
	free_hexp(wlanst);
	free_hexp(cmpbuf);
	return err;
}

/*
int wlan_reconn_ser(void)
{
	int err;
//	int timeout;
	int trycnt = 0;
	char buffer[50];
	uart_close(COMMON_INTERFACE_UART1);
	err = enter_config_mode(460800, 0);

RECONN:
	if (err == 0)
	{
		E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, "AT+TCPDIS=off", -1));
		while(1)
		{
			E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, "AT+TCPLK", -1));
			toupper_arm(buffer);
			if (strstr(buffer, "OFF") != NULL)
				break;
			else
				sleep_ms(800);
		
		}
		E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, "AT+TCPDIS=on", -1));
	}
	
	while(1)
	{
		E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, "AT+TCPLK", -1));
		toupper_arm(buffer);
		if (strstr(buffer, "ON") != NULL)
			break;
		else
			sleep_ms(800);
	}
	
	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, "AT+ENTM", -1));
	
FIN:
	if (err != 0)
	{
		trycnt++;
		sleep_ms(1000);
		printf("wlan_reconn_ser failed %d\n", trycnt);
		if (trycnt < 20)
		{
			uart_close(COMMON_INTERFACE_UART1);
			err = enter_config_mode(460800, 1);
			if (err == 0)
				goto RECONN;
		}
	}
	return err;
}
*/
/*
int init_wlan(char *domain, uint16 serport, int waitconn)
{
	int err;
	char *buffer;
	char *cmpbuf = NULL;
	char *tmp1;
	int timeout;
	int rbuflen = 120;
	int chanset = 0;
	Wlan_WMode_Code wmode_now;
	WlanConfig_ST *wlanst = NULL;
	err = enter_config_mode(460800, 1);
	if (err != 0)
	{
		uart_close(COMMON_INTERFACE_UART1);
		err = enter_config_mode(115200, 1);
	}
	if (err != 0)
		return -1;
	printf("000 after enter_config_mode ret:%d\n", err);
	
	timeout = 2000;
	ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_SET_TIMEOUT, &timeout);
	
	wlanst = malloc(sizeof(WlanConfig_ST));
	cmpbuf = malloc(100);
	buffer = malloc(rbuflen);
	
	sprintf(cmpbuf, "OFF");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+E", -1, 1);
	
	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+WMODE", -1));
	if (strncmp(buffer, "+ok=APSTA", 9) == 0)
		wmode_now = Wlan_WMode_AP_STA;
	else if (strncmp(buffer, "+ok=AP", 6) == 0)
		wmode_now = Wlan_WMode_AP;
	else if (strncmp(buffer, "+ok=STA", 7) == 0)
		wmode_now = Wlan_WMode_STA;

	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+WSMAC", -1));
	tmp1 = strstr(buffer, "=");
	if (tmp1 == NULL)
	{
		err = -1;
		goto FIN;
	}
	E(strTohex(tmp1+1, 12, gWlanMac));
	
	strcpy(cmpbuf, "460800,8,1,NONE,NFC");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+UART", -1, 1);

	printf("get_wlan_config ok Wlan_WMode_STA\n");
	if (Wlan_WMode_STA != wmode_now)
	{
		E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+WMODE=STA", -1));
	}	
			
	sprintf(cmpbuf, "ihexp,12345678hexp");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+WSTA", 5000, 0);

	sprintf(cmpbuf, "DHCP");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+WANN", 5000, 1);
	
	sprintf(cmpbuf, "TCP,CLIENT,8883,172.20.10.7");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+NETP", 5000, 1);
			
	sprintf(cmpbuf, "THROUGHPUT");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+TMODE", 5000, 1);
	
	if (chanset == 1)
	{
//		E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+Z", -1));
//		sleep_ms(500);
//		uart_close(COMMON_INTERFACE_UART1);
		E(enter_config_mode(460800, 1));
	}

	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+ENTM", -1));
	
	timeout = -1;
	ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_SET_TIMEOUT, &timeout);
FIN:
	free(buffer);
	free(wlanst);
	free(cmpbuf);
	return err;
}
*/

int check_4g_runtime(void)
{
	char cmdbuf[50];	
	char buffer[50];
	
	sprintf(cmdbuf, "%sAT+CIP", gUsrCmdPwd);
	if (ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, cmdbuf, -1) == 0)
		return 1;
	else
		return 0;
}

int g4_reconn_ser(void)
{
	int err;
	char cmdbuf[50];	
	char buffer[50];
	
	sprintf(cmdbuf, "%sAT+SOCKAEN=off", gUsrCmdPwd);
	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, cmdbuf, -1));

	sprintf(cmdbuf, "%sAT+SOCKALK", gUsrCmdPwd);
	while(1)
	{
		E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, cmdbuf, -1));
		toupper_arm(buffer);
		if (strstr(buffer, "+SOCKALK:DISCONNECTED") != NULL)
			break;
		else
			sleep_ms(500);
	}
	
	sprintf(cmdbuf, "%sAT+SOCKAEN=on", gUsrCmdPwd);
	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, gUsrCmdPwd, -1));
	
	sprintf(cmdbuf, "%sAT+SOCKALK", gUsrCmdPwd);
	while(1)
	{
		E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, "AT+SOCKALK", -1));
		toupper_arm(buffer);
		if (strstr(buffer, "+SOCKALK:CONNECTED") != NULL)
			break;
		else
			sleep_ms(500);
	}
	
FIN:
	return err;
}

int check_4g(int baud)
{
	char buffer[50];
	int i;
	
	int bauds[3];
	
	set_wlan4g_bauds(baud, bauds);
	
	if (find_uar1_ex_mod(bauds, 3) != 0)
		return -1;
	
	for (i = 0; i < 10; ++i)
	{
		sleep_ms(1500);
		if (ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, "AT+CIP", -1) == 0)
			return 0;
	}

	return -1;
}

uint8 gMonetState = 0;
int init_4g(int baud, char *domain, uint16 serport, int waitconn, int waittm)
{
	int err;
	int chanset = 0;
	char *buffer = NULL;
	char *cmpbuf = NULL;
	int rbuflen = 120;
	char *tmppos1;
	char *tmppos2;
	commonUartPara uart1Para;
	MonetConfig_ST *moset = NULL;
	int bauds[3];
	
	set_wlan4g_bauds(baud, bauds);
	
	if (find_uar1_ex_mod(bauds, 3) != 0)
		return -1;
	TRACE("000 after enter_config_mode ret:%d\n", err);
	
	cmpbuf = malloc_hexp(180);
	buffer = malloc_hexp(rbuflen);
	
	if (domain == NULL)
	{
		int i;
		for (i = 0; i < 10; ++i)
		{
			sleep_ms(1500);
			if (ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, "AT+CIP", -1) == 0)
				break;
		}
		E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 100, "AT+ENTM", -1));
		goto FIN;
	}
//	timeout = 2000;
//	ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_SET_TIMEOUT, &timeout);
	
	sprintf(cmpbuf, "OFF");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+E", -1, 1);
	
	sprintf(cmpbuf, "OFF");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+CACHEN", -1, 1);
	
	sprintf(cmpbuf, "TCP,%s,%d", domain, serport);
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+SOCKA", -1, 0);
	
	sprintf(cmpbuf, "NET");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+WKMOD", -1, 1);
	
	sprintf(cmpbuf, "ON");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+SOCKAEN", -1, 1);
	
	sprintf(cmpbuf, "OFF");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+SOCKBEN", -1, 1);

	sprintf(cmpbuf, "OFF");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+SOCKCEN", -1, 1);

	sprintf(cmpbuf, "OFF");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+SOCKDEN", -1, 1);

	sprintf(cmpbuf, "OFF");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+HEARTEN", -1, 1);

	sprintf(cmpbuf, "OFF");
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+REGEN", -1, 1);

	sprintf(cmpbuf, "%d,8,1,NONE,NONE", baud);
	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+UART", -1, 1);
	
//	sprintf(cmpbuf, "50");
//	IF_ATSend(buffer, rbuflen, cmpbuf, "AT+UARTFT", -1, 1);
	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, rbuflen, "AT+CMDPW", -1));
	tmppos1 = strstr(buffer, ":");
	if (tmppos1 == NULL)
		return -1;
	tmppos2 = strstr(tmppos1, "\r\n");
	strncpy(gUsrCmdPwd, tmppos1+1, tmppos2 - tmppos1-1);
	
	moset = malloc_hexp(sizeof(MonetConfig_ST));
	get_monet_config(moset);
	dump_monet_config(moset);
	if (moset->apn.enable == 1)
	{
		if (moset->apn.cid == 1)
			sprintf(cmpbuf, "%s,%s,%s,%d", moset->apn.name, moset->apn.user, 
				moset->apn.pwd, moset->apn.auth);
		else
			sprintf(cmpbuf, "%s,%s,%s,%d,%d", moset->apn.name, moset->apn.user, 
				moset->apn.pwd, moset->apn.auth, moset->apn.cid);			
		IF_ATSend(buffer, rbuflen, cmpbuf, "AT+APN", -1, 0);
	}
	
	if (chanset == 1)
	{
		E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 100, "AT+S", -1));
		/*
		sleep_ms(500);
		uart_close(COMMON_INTERFACE_UART1);
		E(enter_config_mode(baud, 0));
		*/
	}
	else
	{
		E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 100, "AT+Z", -1));
	}
	sleep_ms(15000);
	ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
	gMonetState = check_4g_runtime();
	sleep_ms(1000);
	
	uart_close(COMMON_INTERFACE_UART1);
	memset(&uart1Para, 0, sizeof(commonUartPara));
	uart1Para.isBlock	= O_BLOCK;
	uart1Para.isPrintf	= 0;
	uart1Para.baudrate	= baud;
	uart1Para.timeout	= 1000;
	uart1Para.isRdam = 1;
	uart_open(COMMON_INTERFACE_UART1, &uart1Para);
	
	if (waitconn == 1)
	{
		char chkconncmd[50];
		uint64 start = getSysTick();

		sprintf(chkconncmd, "%sAT+SOCKALK", gUsrCmdPwd);
		while(1)
		{
			E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, chkconncmd, -1));
			toupper_arm(buffer);
			if (strstr(buffer, "+SOCKALK:CONNECTED") != NULL)
				break;
			else
			{
				if (getSysTick() - start >= waittm)
					return 1;				
				sleep_ms(1000);
			}
		}
	}
FIN:
	if (buffer != NULL)
		free_hexp(buffer);
	if (cmpbuf != NULL)
		free_hexp(cmpbuf);
	if (moset != NULL)
		free_hexp(moset);
	return err;
}

int init_4g_noconf(int baud, int waittime)
{
	int err;
	char *buffer = NULL;
	char *cmpbuf = NULL;
	int rbuflen = 120;
	int i;
	int bauds[3];
	
	set_wlan4g_bauds(baud, bauds);
	
	if (find_uar1_ex_mod(bauds, 3) != 0)
		return -1;
	TRACE("000 after enter_config_mode ret:%d\n", err);
	
	cmpbuf = malloc_hexp(180);
	buffer = malloc_hexp(rbuflen);
		
	for (i = 0; i < waittime/1500; ++i)
	{
		sleep_ms(1500);
		if (ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 50, "AT+CIP", -1) == 0)
			break;
	}
	E(ATSendAndRecv(COMMON_INTERFACE_UART1, buffer, 100, "AT+ENTM", -1));
		
FIN:
	if (buffer != NULL)
		free_hexp(buffer);
	if (cmpbuf != NULL)
		free_hexp(cmpbuf);
	return err;
}







