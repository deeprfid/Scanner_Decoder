
#include "hc32f46_driver.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


/*
//0x1FFF8000
//0x2F000

PendSV_Handler
SysTick_Handler
SVC_Handler
*/
//20026FF0


int user_main_thstk_size =1024;
int user_main_priority = osPriorityNormal;




int is_enable_fwupdate = 1;
int user_main(void)
{
	osStatus_t osst;
	int a = 0;
	int b = 10;
	uint8 flashbuf[300];
	uint8 dnsserver[] = {192,168,0, 1};
	uint8 ftpserver[] = {192,168,1, 44};
	uint8 outip[4];
	int ret;
	int ret2;
	char *pp;
	osThreadAttr_t netThStackAttr;
	void *net_thstk;
	uint8 * dnsbuf;
	int newstksize;
	commonUartPara uart2Para;
	int timeout1 = -1;
	int timeout2 = 1000;

	uart2Para.isBlock	= O_BLOCK;
	uart2Para.isPrintf	= 1;
	uart2Para.baudrate	= 115200;
	uart2Para.timeout	= -1;
	BRDCST_DevInfo devinfo;
	
//	uart_open(COMMON_INTERFACE_UART2, &uart2Para);
	
//	printf("enter firmware_bylib main  __Vectors:%p, __Vectors_End:%p, __Vectors_Size:%p, a:%p, b:%p\n",
//		 &__Vectors, &__Vectors_End, &__Vectors_Size, &a, &b);

	
//	pp = malloc(100);
//	printf("pp:%p\n", pp);

	led_on();
//	InitDegutPrintf(1, COMMON_INTERFACE_SOCKET2, "192.168.1.44", 9999);
	
// W5100S inint//////////////////////////////
	/*
	printf("00000000000000000\n");
	memset(&netThStackAttr, 0, sizeof(netThStackAttr));
	net_thstk = align8byte(malloc(1024*2), 1024*2, &newstksize);
	netThStackAttr.name = "test";
	netThStackAttr.attr_bits = osThreadDetached;
	netThStackAttr.stack_mem = net_thstk;
	netThStackAttr.stack_size = newstksize;
	netThStackAttr.cb_mem = &netth_tcb;
	netThStackAttr.cb_size = sizeof(netth_tcb);
	netThStackAttr.priority = osPriorityNormal;
	
               // Initialize CMSIS-RTOS
	printf("22222222222222222222\n");
	osThreadNew(netThread, NULL, &netThStackAttr);    // Create application main thread
	printf("3333333333333333333\n");
*/
//	ioctl(COMMON_INTERFACE_UART2, COMMON_INTERFACE_UART_SET_TIMEOUT, &timeout2);

//	wait_fin_init();
	/*
	get_network_config(&devinfo.network);
	devinfo.workmode = 2;
	memset(devinfo.bdfwver, 0, 4);
	memset(devinfo.modfwver, 0, 4);
	devinfo.modtype[0] = 0xA1;
	devinfo.modtype[1] = 0x00;
	*/
//	brdcst_conf_init(getMaxSocketId(), &devinfo, BrdCastMode_ThreadMode);
	
	/*
	pre_DNS_init();
	dnsbuf = malloc(1024);
	DNS_init(COMMON_INTERFACE_SOCKET0, dnsbuf);
	ret =  DNS_run(dnsserver, (uint8*)"www.baidu.com", outip);
	printf("DNS_run ret:%d\n",ret);
	if (ret == 1)
		printf("outip:%d.%d.%d.%d\n", outip[0],outip[1],outip[2],outip[3]);
		*/
	/*
	ftpc_init(ftpserver);
	dnsbuf = malloc(2048);
	ftpc_run(dnsbuf);
	*/
	/*
	socket(2, Sn_MR_TCP, 35000, 0x0);
	ret = connect(2, ftpserver, 21);
	printf("connect ret:%d\n", ret);
	ret = write(2, ftpserver, 4);
	printf("write ret:%d\n", ret);
	*/
	printf("app 1.0 0x10000\n");
	/*
	ftpc_init(ftpserver, "", "", "index.html");
	dnsbuf = malloc(1024);
	
	
	while(1)
	{
		
//		read(COMMON_INTERFACE_UART2, flashbuf, 5);
		
//		write(COMMON_INTERFACE_UART2, flashbuf, 5);
		ret = ftpc_run(dnsbuf);
		printf("ftpc_run ret:%d\n", ret);
		if (ret == 226)
		{
			ftpc_destory();
			printf("---------------------------------------------------------------------------------------------------------\n");
			break;
		}
		sleep_ms(100);
//		printf("after read\n");
	}
	*/
	
	/*
		printf("begin test\n");
		timer_Delay_ms(3000);
		
		ret = spiflash_sector_erase(0x7E000);		
		printf("flash_Sector_Erase ret:%d\n", ret);
		timer_Delay_ms(1500);
		
		strcpy((char *)flashbuf, content);
		ret = spiflash_write(0x7E000, flashbuf, 300);
		printf("1 flash_Bytes_Write ret:%d\n", ret);
		
		strcpy((char *)flashbuf, content2);
		ret = spiflash_write(0x7E400, flashbuf, 300);
		printf("2 flash_Bytes_Write ret:%d\n", ret);

		strcpy((char *)flashbuf, content3);
		ret = spiflash_write(0x7EC00, flashbuf, 300);
		printf("3 flash_Bytes_Write ret:%d\n", ret);
		
		timer_Delay_ms(1500);
		memset(flashbuf, 0 , 300);
		spiflash_read(0x7E000, flashbuf, 300);
		printf("11111:  %s\n", (char *)flashbuf);
		
		memset(flashbuf, 0 , 300);
		spiflash_read(0x7E400, flashbuf, 300);
		printf("22222:  %s\n", (char *)flashbuf);
		
		memset(flashbuf, 0 , 300);
		spiflash_read(0x7EC00, flashbuf, 300);
		printf("33333:  %s\n", (char *)flashbuf);
		
	while (1);
	*/
	return 0;
}


