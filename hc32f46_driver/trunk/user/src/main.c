
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "timer.h"
#include "hc32f46_driver.h"
#include "dhcp.h"
#include "driverconfig.h"
#include "port.h"
#include <rt_heap.h>
#define MY_MAX_DHCP_RETRY	3
volatile int gIsDhcpIpSet = 1;
extern volatile int gIsRunDhcpTimeHandler;

int dhcp_wait()
{
	uint8_t res= 0;
	int my_dhcp_retry = 0;
	TRACE("enter dhcp_func\n");
	gIsRunDhcpTimeHandler = 1;
	
	wiz_wait_link_on();
	
	while (1)
	{
		res = DHCP_run();
		switch(res)
		{
			case DHCP_IP_ASSIGN:
//				printf("8888888888888888888888888888888 case DHCP_IP_ASSIGN:\n");
				break;
			case DHCP_IP_CHANGED:
					/* If this block empty, act with default_ip_assign & default_ip_update */
					//
					// This example calls my_ip_assign in the two case.
					//
					// Add to ...
					//
					break;
			case DHCP_IP_LEASED:
				gIsDhcpIpSet = 1;
				return 0;
			case DHCP_FAILED:
				/* ===== Example pseudo code =====  */
				// The below code can be replaced your code or omitted.
				// if omitted, retry to process DHCP
				my_dhcp_retry++;
				if(my_dhcp_retry > MY_MAX_DHCP_RETRY)
				{
					TRACE(">> DHCP %d Failed\r\n", my_dhcp_retry);
					my_dhcp_retry = 0;
					DHCP_stop();      // if restart, recall DHCP_init()
					set_default_ip();
					gIsDhcpIpSet = 1;
					return 0;
//					network_init();   // apply the default static network and print out netinfo to serial
				}
				break;
			default:
				break;
		}
		sleep_ms(200);
//		printf("after DHCP_run:%d\n", res);
	}
}

#if IS_RTOS2_SUPPORT
extern unsigned char Image$$RW_IRAM1$$ZI$$Limit;
extern int user_main_thstk_size;
extern osPriority_t user_main_priority;

void user_main(void *arg);


void dhcp_func(void *arg)
{
	uint8_t res= 0;
	int my_dhcp_retry = 0;
	TRACE("enter dhcp_func\n");
	gIsRunDhcpTimeHandler = 1;
	while (1)
	{
		res = DHCP_run();
		switch(res)
		{
			case DHCP_IP_ASSIGN:
//				printf("8888888888888888888888888888888 case DHCP_IP_ASSIGN:\n");
				break;
			case DHCP_IP_CHANGED:
					/* If this block empty, act with default_ip_assign & default_ip_update */
					//
					// This example calls my_ip_assign in the two case.
					//
					// Add to ...
					//
					break;
			case DHCP_IP_LEASED:
				gIsDhcpIpSet = 1;
//				printf("77777777777777777777777777777777 case DHCP_IP_LEASED:\n");
				//
				// TO DO YOUR NETWORK APPs.
//				loopback_tcpc(SOCK_TCPS, gDATABUF, dest_ip, dest_port);
				break;
			case DHCP_FAILED:
				/* ===== Example pseudo code =====  */
				// The below code can be replaced your code or omitted.
				// if omitted, retry to process DHCP
				my_dhcp_retry++;
				if(my_dhcp_retry > MY_MAX_DHCP_RETRY)
				{
					TRACE(">> DHCP %d Failed\r\n", my_dhcp_retry);
					my_dhcp_retry = 0;
					DHCP_stop();      // if restart, recall DHCP_init()
					set_default_ip();
					gIsDhcpIpSet = 1;
					goto FIN;
//					network_init();   // apply the default static network and print out netinfo to serial
				}
				break;
			default:
				break;
		}
		sleep_ms(200);
//		printf("after DHCP_run:%d\n", res);
	}
	
FIN:
	free_hexp(arg);
}

//osRtxThread_t dhcpth_tcb;
const int dhcp_thstk_size = 1024;
extern int is_enable_fwupdate;
void firmware_upgrade_process(void *arg);
void broadcast_process(void* arg);
extern int gIsConfDhcp;
extern BoardComponents_ST gBoardCompos;

void init_thread(void *arg)
{
//	int ret;
	WorkMode_Code wmode;
	osThreadAttr_t thAttr_t;
	//osThreadId_t thid;
	
	if (get_board_compos() != 0)
	{
		TRACE("if (get_board_compos() != 0)\n");
		//Uart_Ex_Code uart1ex = detect_uart_ex_dev();
		Uart_Ex_Code uart1ex=Uart_Ex_None;
		Spi_Ex_Code spiex = detect_spi_ex_dev();
		TRACE("set_board_compos uart1ex:%d, spiex:%d\n", uart1ex, spiex);
		set_board_compos(spiex, uart1ex);
		sleep_ms(300);
		system_reset();
	}
	else
		TRACE("get_board_compos ok, uart1ex:%d, spiex:%d\n", get_uart_ex_dev(), get_spi_ex_dev());
		
	if (get_spi_ex_dev() == Spi_Ex_Ethernet)
	{
		if (network_init(-1) != 0)
			gBoardCompos.spi_ex = Spi_Ex_None;
		else
		{
			/*
			do{
				sleep_ms(100);
				 if(ctlwizchip(CW_GET_PHYLINK, (void*)&tmp) == -1){
						printf("Unknown PHY Link stauts.\r\n");
				 }
			}while(tmp == PHY_LINK_OFF);
			*/
			TRACE("net init finished gIsConfDhcp:%d\n", gIsConfDhcp);
		//	ret = is_netconf_dhcp(&gNetConf);
			if (gIsConfDhcp == 1)
			{	
				wiz_wait_link_on();
				gIsDhcpIpSet = 0;
				init_osThreadAttr_t(&thAttr_t, dhcp_thstk_size, osPriorityNormal);
				osThreadNew(dhcp_func, NULL, &thAttr_t);
			}
		}
	}

	init_osThreadAttr_t(&thAttr_t, user_main_thstk_size, user_main_priority);
	osThreadNew(user_main, NULL, &thAttr_t);
	if (get_spi_ex_dev() == Spi_Ex_Ethernet)
	{
		wmode = TestFwType_ex();
		TRACE("is_enable_fwupdate:%d,wmode:%d\n", is_enable_fwupdate, wmode);
		if (is_enable_fwupdate == 1 || (is_enable_fwupdate == 2 && wmode > WorkMode_Passive))
		{
			init_osThreadAttr_t(&thAttr_t, 1536, osPriorityNormal);
			osThreadNew(firmware_upgrade_process, NULL, &thAttr_t);
			TRACE("firmware_upgrade_process thid:%p\n", thid);	
		}
		
		TRACE("is_enable_fwupdate:%d\n", is_enable_fwupdate);
	}
	
	broadcast_process(NULL);
}

osRtxThread_t initth_tcb;
int main(void)
{
	int heap_base_address = ((int)&Image$$RW_IRAM1$$ZI$$Limit);
	osThreadAttr_t thAttr_t;
	int init_thstk_size = 1280;
	SWDT_RefreshCounter();
	RCC_Configuration();
	GPIO_Configuration();
	timer_Init();
  TrngInitConfig();
	init_mem_sta();
	heap_base_address += 64 - heap_base_address %64;
	_init_alloc(heap_base_address, 0x20026FF0);

	TRACE("app heap_base_address:%x, init_thstk_size:%p\n", 
		heap_base_address, &init_thstk_size);

	osKernelInitialize();                 // Initialize CMSIS-RTOS
	init_osThreadAttr_t(&thAttr_t, init_thstk_size, osPriorityNormal);
	osThreadNew(init_thread, NULL, &thAttr_t);
	osKernelStart();

	return 0;
}
#endif



