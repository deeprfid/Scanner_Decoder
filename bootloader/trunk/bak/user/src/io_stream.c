#include "uart.h"
#include "hc32f46_driver.h"

int apt_single_select_nob(int sn, int port, uint64 *lastacttime, int acttimeout)
{
	int skstatus;
	int len;
	int ret;
	uint64 now;
	
			skstatus = getSn_SR(sn);
			switch(skstatus)										// 获取socket0的状态
			{
				case SOCK_INIT:	
//					printf("SOCK_INIT:%d\n", sns[t]);
					listen(sn);     							// 监听刚刚打开的本地端口，等待客户端连接
					break;
				case SOCK_ESTABLISHED:							// Socket处于连接建立状态
					if(getSn_IR(sn) & Sn_IR_CON)			
					{
//						printf("6666666666666666666666666666666666\n");
						setSn_IR(sn, Sn_IR_CON);			// Sn_IR的CON位置1，通知W5500连接已建立
						*lastacttime = getSysTick();
					}
//					printf("SOCK_ESTABLISHED :%d\n", sns[t]);

					
					len = getSn_RX_RSR(sn);						// 读取W5500空闲接收缓存寄存器的值并赋给len，Sn_RX_RSR表示接收缓存中已接收和保存的数据大小
//					printf("SOCK_ESTABLISHED len:%d\n", len);
					if(len > 0)
					{
						printf("if(len > 0) :%d\n", sn);
						*lastacttime = getSysTick();
						return sn;
					}		
					
					now = getSysTick();
					if (now - *lastacttime >= acttimeout)
					{
						disconnect(sn);
						close(sn);
					}
					break;					
				case SOCK_CLOSE_WAIT:								// Socket处于等待关闭状态
						  // 此状态仍可以处理收发事务       2019-02-11
					printf("remote close socket SOCK_CLOSE_WAIT:%d\n", sn);
					disconnect(sn);												// ，处理完收发后，发起断开连接命令，以满足4次挥手2019-02-11		
					break;
				case SOCK_CLOSED:										// Socket处于关闭状态
					printf("SOCK_CLOSED:%d\n", sn);
					ret =socket(sn, Sn_MR_TCP, port, Sn_MR_ND);		// 打开Socket0，并配置为TCP无延时模式，打开一个本地端口  2019-02-11
					if (ret < 0)
					{
//						printf("create socket %d error \n", sn);
						return -1;
					}				    
					break;
				default:
//					printf("sock %d default status:%d\n", sns[t], skstatus);
					break;
			}
		return -2;
}
	
int apt_single_select(int sn, int port, uint64 *lastacttime, int acttimeout)
{
	int ret;
	
	while (1)
	{
		ret = apt_single_select_nob(sn, port, lastacttime, acttimeout);
		if (ret != -2)
			return ret;
		osDelay(2);
	}
}

int apt_pair_select_nob(int *sns, int port, int *statusflags)
{
	int t;
	int len;
	int ret;
	int skstatus;

	for(t = 0; t < 2; t++)
	{
			skstatus = getSn_SR(sns[t]);
			switch(skstatus)										// 获取socket0的状态
			{
				case SOCK_INIT:	
//					printf("SOCK_INIT:%d\n", sns[t]);
					listen(sns[t]);     							// 监听刚刚打开的本地端口，等待客户端连接
					break;
				case SOCK_ESTABLISHED:							// Socket处于连接建立状态
					if(getSn_IR(sns[t]) & Sn_IR_CON)			
					{
//						printf("6666666666666666666666666666666666\n");
						setSn_IR(sns[t], Sn_IR_CON);			// Sn_IR的CON位置1，通知W5500连接已建立						
					}
//					printf("SOCK_ESTABLISHED :%d\n", sns[t]);
					if (statusflags[1- t] == 1)
					{
						printf("close previous connection ------------\n");
						disconnect(sns[1- t]);
						close(sns[1- t]);
						statusflags[1- t] = 0;					
					}
					statusflags[t] = 1;
					
					len = getSn_RX_RSR(sns[t]);						// 读取W5500空闲接收缓存寄存器的值并赋给len，Sn_RX_RSR表示接收缓存中已接收和保存的数据大小
//					printf("SOCK_ESTABLISHED len:%d\n", len);
					if(len > 0)
					{
//						printf("if(len > 0) :%d\n", sns[t]);
						return sns[t];
					}		
					break;
				case SOCK_CLOSE_WAIT:								// Socket处于等待关闭状态
						  // 此状态仍可以处理收发事务       2019-02-11
					printf("remote close socket SOCK_CLOSE_WAIT:%d\n", sns[t]);
					disconnect(sns[t]);												// ，处理完收发后，发起断开连接命令，以满足4次挥手2019-02-11		
					statusflags[t] = 0;
					break;
				case SOCK_CLOSED:										// Socket处于关闭状态
					printf("SOCK_CLOSED:%d\n", sns[t]);
					ret =socket(sns[t], Sn_MR_TCP, port, Sn_MR_ND);		// 打开Socket0，并配置为TCP无延时模式，打开一个本地端口  2019-02-11
					if (ret < 0)
					{
						printf("create socket %d error \n", sns[t]);
						return -1;
					}				    
					break;
				default:
//					printf("sock %d default status:%d\n", sns[t], skstatus);
					break;
			}
		}
		
	return -2;
}
	
int apt_pair_select(int *sns, int port, int *statusflags)
{
	int ret;

	while (1)
	{
		ret = apt_pair_select_nob(sns, port, statusflags);
		if (ret != -2)
			return ret;
		osDelay(1);
	}
}

commonSocketPara gSocketParams[TOTAL_SOCKET_NUM];



extern commonUartParaLocal gUartParams[TOTAL_UART_NUM];


int uart_open(int uart, void *paraAddr)
{
	int ret = -1;
	commonUartParaLocal	*paraLoc;
	commonUartPara *srcPara;

	if(paraAddr == NULL)
	{
		printf("uart_open--paraAddr == NULL\n");
		return -1;
	}
	
	switch(uart)
	{
		case COMMON_INTERFACE_UART0:
		case COMMON_INTERFACE_UART1:
		case COMMON_INTERFACE_UART2:
		case COMMON_INTERFACE_UART3:
		{
			paraLoc = &gUartParams[uart - COMMON_INTERFACE_BASE];
			if(paraLoc->isOpen != 1)
			{
				srcPara = (commonUartPara *)paraAddr;
				ret =hc32f460_uart_init(uart - COMMON_INTERFACE_BASE + 1, srcPara->baudrate);

				if(ret >= 0)
				{
					paraLoc->basepara.databits	= srcPara->databits;
					paraLoc->basepara.mode		= srcPara->mode;
					paraLoc->basepara.stopbits	= srcPara->stopbits;
					paraLoc->basepara.baudrate	= srcPara->baudrate;
					paraLoc->basepara.isBlock	= srcPara->isBlock;
					paraLoc->basepara.isPrintf	= srcPara->isPrintf;
					paraLoc->basepara.timeout	= srcPara->timeout;
					paraLoc->isOpen	= 1;
					ret = uart;
				}
				else
				{
					printf("uart_open--hc32f460_uart_init err:%d\n", ret);
					return -1;
				}
			}
			break;
		}
		default:
		{
			printf("uart_open--invalid interface number\n");
			return -1;
		}
	}
	
	return ret;
}

int uart_close(int uart)
{
	int ret = -1;
	commonUartParaLocal	*paraLoc;
	
	switch(uart)
	{
		case COMMON_INTERFACE_UART0:
		case COMMON_INTERFACE_UART1:
		case COMMON_INTERFACE_UART2:
		case COMMON_INTERFACE_UART3:
		{
			paraLoc = &gUartParams[uart - COMMON_INTERFACE_BASE];
			ret = hc32f460_init_uart_close(uart - COMMON_INTERFACE_BASE);
			if (ret != 0)
			{
				printf("uart_close--hc32f460_init_uart_close err:%d\n", ret);
				return -1;
			}
			paraLoc->isOpen	= 0;
			break;
		}
		default:
		{
			printf("uart_close--invalid interface number\n");
			return -1;
		}
	}

	return ret;
}

extern int gEnableUart3Printf;
int ioctl(int s, uint32 cmd, void *paraAddr)
{
	commonUartParaLocal	*paraLoc;
//	commonUartPara srcPara;
	uint32	value;
	int ret = -1;

	if(paraAddr == NULL)
	{
		printf("ioctl--paraAddr == NULL\n");
		return -1;
	}
	
	switch(s)
	{
		case COMMON_INTERFACE_UART0:
		case COMMON_INTERFACE_UART1:
		case COMMON_INTERFACE_UART2:
		case COMMON_INTERFACE_UART3:
		{
			value = *((uint32 *)paraAddr);
			paraLoc = &gUartParams[s - COMMON_INTERFACE_BASE];
			switch(cmd)
			{
				case COMMON_INTERFACE_UART_SET_ALLPARA:
					ret = uart_close(s);
					if (ret != 0)
					{
						printf("ioctl--uart_close err:%d\n", ret);
						return -1;
					}
					ret = uart_open(s, paraAddr);
					if (ret != 0)
					{
						printf("ioctl--uart_open err:%d\n", ret);
						return -1;
					}
					ret =  0;
					break;
				case COMMON_INTERFACE_UART_SET_ISBLOCK:
					paraLoc->basepara.isBlock	= value;
					ret =  0;
					break;
				case COMMON_INTERFACE_UART_SET_TIMEOUT:
					paraLoc->basepara.timeout	= value;
					//printf("SET_TIMEOUT uart%d = %d",s-COMMON_INTERFACE_UART0,para->timeout);
					ret =  0;
					break;
				case COMMON_INTERFACE_UART_SET_ISPRINTF:
					if(s == COMMON_INTERFACE_UART3)
					{
						paraLoc->basepara.isPrintf	= value;
						gEnableUart3Printf = value;
						ret =  0;
					}
					break;
				case COMMON_INTERFACE_UART_CLEAR_REVBUF:
					paraLoc->uart_head = paraLoc->uart_tail;
					ret =  0;
					break;
				case COMMON_INTERFACE_UART_SET_BAUDRATE:
					paraLoc->basepara.baudrate		= value;
					ret = uart_close(s);
					if (ret != 0)
					{
						printf("ioctl--uart_close err:%d\n", ret);
						return -1;
					}				
					ret = uart_open(s, &paraLoc->basepara);//maybe problem hexp
					if (ret != 0)
					{
						printf("ioctl--uart_open err:%d\n", ret);
						return -1;
					}	
					ret =  0;
					break;
				default:
					{
						printf("ioctl--invalid command\n");
						return -1;
					}
			}
			break;
		}
		case COMMON_INTERFACE_SOCKET0:
		case COMMON_INTERFACE_SOCKET1:
		case COMMON_INTERFACE_SOCKET2:
		case COMMON_INTERFACE_SOCKET3:
		{
			ret = ctlsocket(s, (ctlsock_type)cmd, (void *)paraAddr);
			if (ret != SOCK_OK)
			{
				printf("ioctl--ctlsocket error:%d\n", ret);
				return -1;
			}
			else
				ret = 0;
			break;
		}
		default:
		{
			printf("ioctl--invalid interface number\n");
			return -1;
		}	
	}
	
	return ret;
}

int read(int s, void *buf, uint32 len)
{
	int ret = -1,ret0;
	int nsockrecv;
//	commonUartParaLocal	*para;
	int tmcnt;
	
	if(buf == NULL)
	{
		printf("read--buf == NULL\n");
		return -1;
	}
	
	switch(s)
	{
		case COMMON_INTERFACE_SOCKET0:
		case COMMON_INTERFACE_SOCKET1:
		case COMMON_INTERFACE_SOCKET2:
		case COMMON_INTERFACE_SOCKET3:
		{
			commonSocketPara *skpara = &gSocketParams[s];
			if(skpara->isBlock != O_NONBLOCK)
			{
				int skstatus;
				tmcnt = skpara->timeout;				
				while (1)
				{
					skstatus = getSn_SR(s);
					if (skstatus == SOCK_ESTABLISHED || 
						skstatus == SOCK_CLOSE_WAIT)
						nsockrecv = getSn_RX_RSR(s);
					else
					{
						printf("read--skstatus error\n");
						return -1;
					}
					if (nsockrecv > 0)
					{
						ret = recv(s, buf, nsockrecv);
						if (ret < 0)
						{
							printf("read--wizchip recv err:%d\n", ret);
							return -1;
						}
						break;
					}
					else
				  {
					  
					  	if (skpara->timeout > 0)
					  {
						  	if (tmcnt <= 0)
								return -2;
					  }
					  osDelay(1);
					  if (skpara->timeout > 0)
						  tmcnt -= SYSTEM_TICK_DUR;
				  }					  
				}
			}
			else
			{
				ret = recv(s, buf, nsockrecv);
				if (ret == SOCK_BUSY)
					ret = 0;
			}
			break;
		}
		case COMMON_INTERFACE_UART0:
		case COMMON_INTERFACE_UART1:
		case COMMON_INTERFACE_UART2:
		case COMMON_INTERFACE_UART3:
		{
			commonUartParaLocal *uartpara = &gUartParams[s - COMMON_INTERFACE_BASE];
			int tmpret;
			
			if(uartpara->isOpen == 0)
			{
				printf("read--uart is not open\n");
				return -1;
			}
					
			if(uartpara->basepara.isBlock != O_NONBLOCK)
			{
				tmcnt = uartpara->basepara.timeout;
				while (1)
				{
					tmpret = hc32f460_uart_get_bytes_cnt(s, &uartpara->uart_head);
					if (tmpret != 0)
					{
						printf("read--hc32f460_uart_get_bytes_cnt err:%d\n", tmpret);
						return -1;
					}
					if(uartpara->uart_head != uartpara->uart_tail)
					{
						ret = uart_recv(s, buf, len);
						break;
					}
					else
					{
						if (uartpara->basepara.timeout > 0)
						{
							if (tmcnt <= 0)
								return -2;
						}
						osDelay(1);
						if (uartpara->basepara.timeout > 0)
							tmcnt -= SYSTEM_TICK_DUR;						
					}
				}
			}
			else
			{
				tmpret = hc32f460_uart_get_bytes_cnt(s, &uartpara->uart_head);
				if (tmpret != 0)
				{
					printf("read--hc32f460_uart_get_bytes_cnt err:%d\n", tmpret);
					return -1;
				}
				ret = uart_recv(s, buf, len);
			}
			break;	
		}			
		default:
		{
			printf("read--invalid interface number\n");
			return -1;
		}
	}
	return ret;
}

int write(int s, const void *buf, uint32 len)
{
	int ret = -1;

	if(buf == NULL)
	{
		printf("write--buf == NULL\n");
		return -1;
	}

	switch(s)
	{
		case COMMON_INTERFACE_SOCKET0:
		case COMMON_INTERFACE_SOCKET1:
		case COMMON_INTERFACE_SOCKET2:
		case COMMON_INTERFACE_SOCKET3:
		{
			int skstatus;
			
			if (skstatus == SOCK_ESTABLISHED || 
				skstatus == SOCK_CLOSE_WAIT)
			{
				ret = send(s, (uint8*)buf, len);
				
				if (ret == SOCK_BUSY)
					ret = 0;
				else if (ret < 0)
				{
					printf("write--wizchip send error:%d\n", ret);
					return -1;
				}					
			}
			else
			{
				printf("write--socket status error\n");
				return -1;
			}
			break;
		}
		case COMMON_INTERFACE_UART0:
		case COMMON_INTERFACE_UART1:
		case COMMON_INTERFACE_UART2:
		case COMMON_INTERFACE_UART3:
		{
			commonUartParaLocal *uartpara = &gUartParams[s - COMMON_INTERFACE_BASE];
			if(uartpara->isOpen == 0)
			{
				printf("write--uart is not open\n");
				return -1;
			}
			ret = uart_send(s, buf, len);
			break;
		}
		default:
		{
			printf("write--invalid interface number\n");
			return -1;
		}
	}

	return ret;
}

void os_dly_wait(int tenmscnt)
{
	osDelay(tenmscnt*2);
}