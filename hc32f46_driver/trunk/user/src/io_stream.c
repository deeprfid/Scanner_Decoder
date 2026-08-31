#include "uart.h"
#include "os_dep.h"
#include "pio.h"
#include "io_stream.h"
#include "driverconfig.h"

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
//						printf("if(len > 0) :%d\n", sn);
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
//					printf("remote close socket SOCK_CLOSE_WAIT:%d\n", sn);
					disconnect(sn);												// ，处理完收发后，发起断开连接命令，以满足4次挥手2019-02-11		
					break;
				case SOCK_CLOSED:										// Socket处于关闭状态
//					printf("SOCK_CLOSED:%d\n", sn);
					ret =socket(sn, Sn_MR_TCP, port, SF_TCP_NODELAY | SF_IO_NONBLOCK);		// 打开Socket0，并配置为TCP无延时模式，打开一个本地端口  2019-02-11
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
		sleep_ms(20);
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
						TRACE("close previous connection ------------\n");
						disconnect(sns[1- t]);
						close(sns[1- t]);
						statusflags[1- t] = 0;					
					}
					statusflags[t] = 1;
					
					len = getSn_RX_RSR(sns[t]);						// 读取W5500空闲接收缓存寄存器的值并赋给len，Sn_RX_RSR表示接收缓存中已接收和保存的数据大小
//					printf("SOCK_ESTABLISHED len:%d\n", len);
					if(len > 0)
					{
//						printf("if(len > 0) :%d, len:%d\n", sns[t], len);
						return sns[t];
					}		
					break;
				case SOCK_CLOSE_WAIT:								// Socket处于等待关闭状态
						  // 此状态仍可以处理收发事务       2019-02-11
//					printf("remote close socket SOCK_CLOSE_WAIT:%d\n", sns[t]);
					disconnect(sns[t]);										// ，处理完收发后，发起断开连接命令，以满足4次挥手2019-02-11		
					close(sns[t]);
					statusflags[t] = 0;
					break;
				case SOCK_CLOSED:										// Socket处于关闭状态
//					printf("SOCK_CLOSED:%d\n", sns[t]);
					statusflags[t] = 0;
					ret =socket(sns[t], Sn_MR_TCP, port, SF_TCP_NODELAY | SF_IO_NONBLOCK);		// 打开Socket0，并配置为TCP无延时模式，打开一个本地端口  2019-02-11
					if (ret < 0)
					{
						TRACE("create socket %d error \n", sns[t]);
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

int apt_pair_select_nob_ex(apt_pair_socks_st *apt_st)
{
	int t;
	int len;
	int ret;
	int skstatus;
	
	for(t = 0; t < 2; t++)
	{
			skstatus = getSn_SR(apt_st->sns[t]);
			switch(skstatus)										// 获取socket0的状态
			{
				case SOCK_INIT:	
//					printf("SOCK_INIT:%d\n", sns[t]);
					listen(apt_st->sns[t]);     							// 监听刚刚打开的本地端口，等待客户端连接
					break;
				case SOCK_ESTABLISHED:							// Socket处于连接建立状态
					if(getSn_IR(apt_st->sns[t]) & Sn_IR_CON)	
					{
//						printf("6666666666666666666666666666666666\n");
						setSn_IR(apt_st->sns[t], Sn_IR_CON);			// Sn_IR的CON位置1，通知W5500连接已建立	
						if (apt_st->create_conn_cb != NULL)
							apt_st->create_conn_cb();
					}
//					printf("SOCK_ESTABLISHED :%d\n", sns[t]);
					if (apt_st->statusflags[1- t] == 1)
					{
//						printf("close previous connection start t:%d\n", apt_st->sns[1- t]);
						disconnect(apt_st->sns[1- t]);
//						printf("close previous connection end t:%d\n", apt_st->sns[1- t]);
						close(apt_st->sns[1- t]);
						apt_st->statusflags[1- t] = 0;					
					}
					apt_st->statusflags[t] = 1;
					
					len = getSn_RX_RSR(apt_st->sns[t]);						// 读取W5500空闲接收缓存寄存器的值并赋给len，Sn_RX_RSR表示接收缓存中已接收和保存的数据大小
//					printf("SOCK_ESTABLISHED len:%d\n", len);
					if(len > 0)
					{
//						printf("if(len > 0) :%d, len:%d\n", sns[t], len);
						return apt_st->sns[t];
					}		
					break;
				case SOCK_CLOSE_WAIT:								// Socket处于等待关闭状态
						  // 此状态仍可以处理收发事务       2019-02-11
//					printf("start remote close socket SOCK_CLOSE_WAIT:%d\n", apt_st->sns[t]);
					disconnect(apt_st->sns[t]);										// ，处理完收发后，发起断开连接命令，以满足4次挥手2019-02-11	
//					printf("end remote close socket SOCK_CLOSE_WAIT:%d\n", apt_st->sns[t]);
					close(apt_st->sns[t]);
					apt_st->statusflags[t] = 0;
					if (apt_st->statusflags[1-t] == 0)
					{
						if (apt_st->close_conn_cb != NULL)
							apt_st->close_conn_cb();
					}
					break;
				case SOCK_CLOSED:										// Socket处于关闭状态
//					printf("SOCK_CLOSED:%d\n", apt_st->sns[t]);
					apt_st->statusflags[t] = 0;
					if (apt_st->statusflags[1-t] == 0)
					{
						if (apt_st->close_conn_cb != NULL)
							apt_st->close_conn_cb();
					}				
					ret =socket(apt_st->sns[t], Sn_MR_TCP, apt_st->port, SF_TCP_NODELAY | SF_IO_NONBLOCK);		// 打开Socket0，并配置为TCP无延时模式，打开一个本地端口  2019-02-11
					if (ret < 0)
					{
						TRACE("create socket %d error \n", apt_st->sns[t]);
						return -1;
					}				    
					break;
				default:
//					printf("sock %d default status:%d\n", apt_st->sns[t], skstatus);
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
		sleep_ms(5);
	}
}

int apt_pair_select_ex(apt_pair_socks_st *apt_st)
{
	int ret;
	while (1)
	{
		ret = apt_pair_select_nob_ex(apt_st);
		if (ret != -2)
			return ret;
		sleep_ms(5);
	}
}


extern commonSocketPara gSocketParams[];
extern commonUartParaLocal gUartParams[TOTAL_UART_NUM];

#define COMMON_INTERFACE_UART_BASE    100
#define COMMON_INTERFACE_USB_BASE    110

int uart_open(int uart, void *paraAddr)
{
	int ret = -1;
	commonUartParaLocal	*paraLoc;
	commonUartPara *srcPara;

	if(paraAddr == NULL)
	{
		TRACE("uart_open--paraAddr == NULL\n");
		return -1;
	}
	
	switch(uart)
	{
		case COMMON_INTERFACE_UART0:
		case COMMON_INTERFACE_UART1:
		case COMMON_INTERFACE_UART2:
		case COMMON_INTERFACE_UART3:
		{
			int intuid = uart - COMMON_INTERFACE_UART_BASE;
			paraLoc = &gUartParams[intuid];
			if(paraLoc->isOpen != 1)
			{
				uart_cfg_para_st ucpst;				
				srcPara = (commonUartPara *)paraAddr;
				ucpst.uartid = intuid;
				ucpst.baud = srcPara->baudrate;
				ucpst.databits = srcPara->databits;
				ucpst.stopbits = srcPara->stopbits;
				ucpst.flowctol = srcPara->flowctrl;
				ucpst.parity = srcPara->parity;
				ucpst.isrdma = srcPara->isRdam;
				ucpst.t485 = srcPara->t485;
				ret =hc32f460_uart_init(&ucpst);

				if(ret >= 0)
				{
					paraLoc->basepara = *srcPara;
					paraLoc->isOpen = 1;
					paraLoc->uart_head = 0;
					paraLoc->uart_tail = 0;
					if (srcPara->t485 == 1 && uart == COMMON_INTERFACE_UART3)
						RS485_set_rec();
					ret = uart;
					TRACE("init uart successfule\n");
				}
				else
				{
					TRACE("uart_open--hc32f460_uart_init err:%d\n", ret);
					return -1;
				}
			}
			break;
		}
		default:
		{
			TRACE("uart_open--invalid interface number\n");
			return -1;
		}
	}
	
	return uart;
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
			int intuid = uart - COMMON_INTERFACE_UART_BASE;
			paraLoc = &gUartParams[intuid];
			ret = hc32f460_init_uart_close(intuid);
			if (ret != 0)
			{
				TRACE("uart_close--hc32f460_init_uart_close err:%d\n", ret);
				return -1;
			}
			paraLoc->isOpen = 0;
			break;
		}
		default:
		{
			TRACE("uart_close--invalid interface number\n");
			return -1;
		}
	}

	return ret;
}

commonUsbParaLocal gUsbParams[2];
int usb_recv(int uid, void *buf, uint32_t len);
int usb_send(int uid, const void *buf, uint32_t len);

int ioctl(int s, uint32 cmd, void *paraAddr)
{
	commonUartParaLocal	*paraLoc;
	int value;
	int ret = -1;
	
	if (cmd != COMMON_INTERFACE_CLEAR_REVBUF)
	{
		if(paraAddr == NULL)
		{
			TRACE("ioctl--paraAddr == NULL\n");
			return -1;
		}
	}
	
	switch(s)
	{
		case COMMON_INTERFACE_UART0:
		case COMMON_INTERFACE_UART1:
		case COMMON_INTERFACE_UART2:
		case COMMON_INTERFACE_UART3:
		{
			int intuid = s - COMMON_INTERFACE_UART_BASE;
			value = *((int *)paraAddr);
			paraLoc = &gUartParams[intuid];
			switch(cmd)
			{
				case COMMON_INTERFACE_SET_ALLPARA:
					ret = uart_close(s);
					if (ret != 0)
					{
						TRACE("ioctl--uart_close err:%d\n", ret);
						return -1;
					}
					ret = uart_open(s, paraAddr);
					if (ret != s)
					{
						TRACE("ioctl--uart_open err:%d\n", ret);
						return -1;
					}
					ret =  0;
					break;
				case COMMON_INTERFACE_SET_ISBLOCK:
					paraLoc->basepara.isBlock	= value;
					ret =  0;
					break;
				case COMMON_INTERFACE_SET_TIMEOUT:
					paraLoc->basepara.timeout	= value;
//					printf("SET_TIMEOUT uart%d = %d\n",intuid, paraLoc->basepara.timeout);
					ret =  0;
					break;
				case COMMON_INTERFACE_SET_ISPRINTF:
					if(s == COMMON_INTERFACE_UART3)
					{
						paraLoc->basepara.isPrintf	= value;
						ret =  0;
					}
					break;
				case COMMON_INTERFACE_CLEAR_REVBUF:
					hc32f460_uart_clear_buf(intuid, paraLoc->basepara.isRdam);
					paraLoc->uart_head = 0;
					paraLoc->uart_tail = 0;
					ret =  0;
					break;
				case COMMON_INTERFACE_SET_BAUDRATE:
					paraLoc->basepara.baudrate		= value;
					ret = uart_close(s);
					if (ret != 0)
					{
						TRACE("ioctl--uart_close err:%d\n", ret);
						return -1;
					}				
					ret = uart_open(s, &paraLoc->basepara);//maybe problem hexp
					if (ret != s)
					{
						TRACE("ioctl--uart_open err:%d\n", ret);
						return -1;
					}	
					ret =  0;
					break;
				case COMMON_INTERFACE_PEEK_DATA_SIZE:
					paraLoc->uart_tail = hc32f460_uart_get_bytes_cnt(intuid, 
						paraLoc->basepara.isRdam);
					value = paraLoc->uart_tail - paraLoc->uart_head;
					if (value < 0)
						value += paraLoc->recvbufsize;
					*((int *)paraAddr) = value;
					break;
				default:
					{
						TRACE("ioctl--invalid command\n");
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
			commonSocketPara *skpara = &gSocketParams[s];
			value = *((int *)paraAddr);
			switch(cmd)
			{
				case COMMON_INTERFACE_SET_ISBLOCK:
					skpara->isBlock = value;
					ret =  0;
					break;			
				case COMMON_INTERFACE_SET_TIMEOUT:
					skpara->timeout = value;
					ret =  0;
					break;
				default:
					return -1;
				/*
					ret = ctlsocket(s, (ctlsock_type)cmd, (void *)paraAddr);
					if (ret != SOCK_OK)
					{
						TRACE("ioctl--ctlsocket error:%d\n", ret);
						return -1;
					}
					else
						ret = 0;
					break;
				*/
			}
			break;
		}
		case COMMON_INTERFACE_USB0:
		case COMMON_INTERFACE_USB1:
		{
			int usbid = s - COMMON_INTERFACE_USB_BASE;
			commonUsbParaLocal *ubpara = &gUsbParams[usbid];
			value = *((int *)paraAddr);
			switch(cmd)
			{
				case COMMON_INTERFACE_SET_ISBLOCK:
					ubpara->isBlock = value;
					ret =  0;
					break;
				case COMMON_INTERFACE_SET_TIMEOUT:
					ubpara->timeout = value;
					ret =  0;
					break;
				case COMMON_INTERFACE_CLEAR_REVBUF:
					ubpara->usb_head = 0;
					ubpara->usb_tail = 0;
					ret =  0;
					break;
				case COMMON_INTERFACE_PEEK_DATA_SIZE:
					value = ubpara->usb_tail - ubpara->usb_head;
					if (value < 0)
						value += ubpara->recvbufsize;
					*((int *)paraAddr) = value;
					break;	
				default:					
					{
						TRACE("ioctl--invalid command\n");
						return -1;
					}
			}
			break;
		}
		default:
		{
			TRACE("ioctl--invalid interface number\n");
			return -1;
		}	
	}
	
	return ret;
}
//int lastuartbytescnt = -1;
extern int gIsUsbAvailable;
extern int gUsbCompType;
int apt_uart_select_nob(int *uarts, int ucnt)
{
	int i;
	int uartid;
	
	for (i = 0; i < ucnt; ++i)
	{	
		uartid = uarts[i] - COMMON_INTERFACE_UART_BASE;
		if (gUartParams[uartid].isOpen == 0)
			continue;
		gUartParams[uartid].uart_tail = hc32f460_uart_get_bytes_cnt(uartid, gUartParams[uartid].basepara.isRdam);
		if (gUartParams[uartid].uart_head != gUartParams[uartid].uart_tail)
			return uarts[i];
	}
	return 0;
}
/*
void dumpUsb(int flag)
{
	if (flag == 0)
		printf("0000 wi:%d, ri:%d\n", gUsbParams[0].usb_tail, gUsbParams[0].usb_head);
	else
		printf("1111 wi:%d, ri:%d\n", gUsbParams[0].usb_tail, gUsbParams[0].usb_head);
}
*/
int apt_usb_select_nob(void)
{
	int i;
	int infcnt = 1;
	if (gIsUsbAvailable != 1)
		return 0;
	if (gUsbCompType == 2)
		infcnt = 2;
	
	for (i = 0; i < infcnt; ++i)
	{
		if (gUsbParams[i].usb_head != gUsbParams[i].usb_tail)
			return i+COMMON_INTERFACE_USB_BASE;
	}
	return 0;
}

int apt_usb_select(void)
{
	int usbinf;
	while (1)
	{
		usbinf = apt_usb_select_nob() ;
		if (usbinf == 0)
			continue;
		else if (usbinf > 0)
			return usbinf;
		sleep_ms(5);
	}
	return 0;
}
int apt_sockets_select(int *socks, int sockcnt)
{
	int i;
	int skstatus;
	for (i = 0; i < sockcnt; ++i)
	{
		skstatus = getSn_SR(socks[i]);
		if (skstatus == SOCK_ESTABLISHED || 
			skstatus == SOCK_CLOSE_WAIT || 
			skstatus == SOCK_UDP)
		{
			if (getSn_RX_RSR(socks[i]) > 0)
				return socks[i];
		}
	}
	return -1;
}

int apt_multi_infs_select_nob(apt_pair_socks_st *apt_st, int *uarts, 
	int uartcnt, int *socks, int sockcnt)
{
	int fd;
	
	if (apt_st != NULL)
	{
		fd = apt_pair_select_nob_ex(apt_st);
		if (fd > -1)
			return fd;
	}
	if (socks != NULL && sockcnt > 0)
	{
		fd = apt_sockets_select(socks, sockcnt);
		if (fd > -1)
			return fd;
	}
	if (uarts != NULL && uartcnt > 0)
	{
		fd = apt_uart_select_nob(uarts, uartcnt);
		if (fd > 0)
			return fd;
	}
	fd = apt_usb_select_nob();
	if (fd > 0)
		return fd;
	
	return -1;
}

int apt_multi_infs_select(apt_pair_socks_st *apt_st, int *uarts, 
	int uartcnt, int *socks, int sockcnt)
{
	int fd = -2;
//	int ret = 0;
	
	while (1)
	{
		fd = apt_multi_infs_select_nob(apt_st, uarts, uartcnt, socks, sockcnt);
		if (fd >= 0)
			return fd;
		sleep_ms(5);
		/*
		if (apt_st != NULL)
		{
			fd = apt_pair_select_nob_ex(apt_st);
			if (fd > -1)
				return fd;
		}
		
		if (socks != NULL)
		{
			fd = apt_sockets_select(socks, sockcnt);
			if (fd > -1)
				return fd;
		}
		
		if (uarts != NULL && uartcnt > 0)
		{
			fd = apt_uart_select_nob(uarts, uartcnt);
			if (fd > 0)
				return fd;
			else if (fd < 0)
			{
				ret = -1;
				break;
			}
		}
		
		fd = apt_usb_select_nob();
		if (fd > 0)
			return fd;
		else if (fd < 0)
		{
			ret = -2;
			break;
		}
		sleep_ms(5);
		*/
	}
	
	return -1;
}


int read(int s, void *buf, uint32 len)
{
	int ret = -1;
	int nsockrecv;
	int nread;
//	commonUartParaLocal	*para;
	int tmcnt;
	
	if (len == 0)
		return 0;
	
	if(buf == NULL)
	{
		TRACE("read--buf == NULL\n");
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
			uint8 dummyaddr[4];
			uint16 dummyport;			
			commonSocketPara *skpara = &gSocketParams[s];
			if(skpara->isBlock == O_BLOCK)
			{				
				tmcnt = skpara->timeout;				
				while (1)
				{
					skstatus = getSn_SR(s);
					if (skstatus == SOCK_ESTABLISHED || 
						skstatus == SOCK_CLOSE_WAIT || 
						skstatus == SOCK_UDP)
						nsockrecv = getSn_RX_RSR(s);
					else
					{
						TRACE("read--skstatus error\n");
						return -1;
					}
					if (nsockrecv > 0)
					{
						if (nsockrecv >= len)
							nread = len;
						else
							nread = nsockrecv;
						if (skstatus == SOCK_UDP)
							ret = recvfrom(s, buf, nread, dummyaddr, &dummyport);
						else
							ret = recv(s, buf, nread);
						if (ret < 0)
						{
							TRACE("read--wizchip recv err:%d\n", ret);
							return -1;
						}
						break;
					}
					else
					{					 
					  if (skpara->timeout >= 0)
					  {
						  	if (tmcnt <= 0)
								return -2;
					  }
					  sleep_ms(5);
					  if (skpara->timeout >= 0)
						  tmcnt -= SYSTEM_TICK_DUR;
				  }					  
				}
			}
			else
			{
				skstatus = getSn_SR(s);
				nsockrecv = getSn_RX_RSR(s);
				if (nsockrecv > 0)
				{
					if (skstatus == SOCK_UDP)
						ret = recvfrom(s, buf, len, dummyaddr, &dummyport);
					else
						ret = recv(s, buf, len);
				}
				else
					ret = 0;
			}
			break;
		}
		case COMMON_INTERFACE_UART0:
		case COMMON_INTERFACE_UART1:
		case COMMON_INTERFACE_UART2:
		case COMMON_INTERFACE_UART3:
		{
			int intuid = s - COMMON_INTERFACE_UART_BASE;
			commonUartParaLocal *uartpara = &gUartParams[intuid];
						
			if(uartpara->isOpen == 0)
			{
				TRACE("read--uart is not open\n");
				return -1;
			}
					
			if(uartpara->basepara.isBlock == O_BLOCK)
			{
				tmcnt = uartpara->basepara.timeout;
				while (1)
				{
					uartpara->uart_tail = hc32f460_uart_get_bytes_cnt(intuid, 
						uartpara->basepara.isRdam);
					if(uartpara->uart_head != uartpara->uart_tail)
					{
						ret = uart_recv(intuid, buf, len);
						break;
					}
					else
					{
						if (uartpara->basepara.timeout >= 0)
						{
							if (tmcnt <= 0)
								return -2;
						}
						sleep_ms(5);
						if (uartpara->basepara.timeout >= 0)
							tmcnt -= 5;						
					}
				}
			}
			else
			{
				uartpara->uart_tail = hc32f460_uart_get_bytes_cnt(intuid, 
					uartpara->basepara.isRdam);
				ret = uart_recv(intuid, buf, len);
			}
			break;	
		}
		case COMMON_INTERFACE_USB0:
		case COMMON_INTERFACE_USB1:	
		{
			int usbid = s - COMMON_INTERFACE_USB_BASE;
			commonUsbParaLocal *ubpara = &gUsbParams[usbid];

			if(ubpara->isBlock == O_BLOCK)
			{
				tmcnt = ubpara->timeout;
				while (1)
				{
					if(ubpara->usb_head != ubpara->usb_tail)
					{
						ret = usb_recv(usbid, buf, len);
						break;
					}
					else
					{
						if (ubpara->timeout > 0)
						{
							if (tmcnt <= 0)
								return -2;
						}
						sleep_ms(5);
						if (ubpara->timeout > 0)
							tmcnt -= 5;						
					}
				}				
			}
			else
				ret = usb_recv(usbid, buf, len);
			break;
		}
		default:
		{
			TRACE("read--invalid interface number\n");
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
		TRACE("write--buf == NULL\n");
		return -1;
	}

	switch(s)
	{
		case COMMON_INTERFACE_SOCKET0:
		case COMMON_INTERFACE_SOCKET1:
		case COMMON_INTERFACE_SOCKET2:
		case COMMON_INTERFACE_SOCKET3:
		{
			int skstatus = getSn_SR(s);
			if (skstatus == SOCK_ESTABLISHED || 
				skstatus == SOCK_CLOSE_WAIT)
			{
				ret = send(s, (uint8*)buf, len);
				
				if (ret == SOCK_BUSY)
					ret = 0;
				else if (ret < 0)
				{
					TRACE("write--wizchip send error:%d\n", ret);
					return -1;
				}
			}
			else
			{
				TRACE("write--socket status error:%d\n", skstatus);
				return -1;
			}
			break;
		}
		case COMMON_INTERFACE_UART0:
		case COMMON_INTERFACE_UART1:
		case COMMON_INTERFACE_UART2:
		case COMMON_INTERFACE_UART3:
		{
			int intuid = s - COMMON_INTERFACE_UART_BASE;
			commonUartParaLocal *uartpara = &gUartParams[intuid];
			if(uartpara->isOpen == 0)
			{
				TRACE("write--uart is not open\n");
				return -1;
			}
			ret = uart_send(intuid, buf, len, uartpara->basepara.t485);
			break;
		}
		case COMMON_INTERFACE_USB0:
		case COMMON_INTERFACE_USB1:
		{
			int usbid = s - COMMON_INTERFACE_USB_BASE;
			ret = usb_send(usbid, buf, len);
			break;
		}
		default:
		{
			TRACE("write--invalid interface number\n");
			return -1;
		}
	}

	return ret;
}

void os_dly_wait(int tenmscnt)
{
	sleep_ms(tenmscnt*(10/SYSTEM_TICK_DUR));
}


int read_n(int s, void *buf, uint32 len)
{
	int pos = 0;
	int nleft = len;
	int nret = 0;
	unsigned char *pbuf = buf;
	
	while (nleft > 0)
	{
		nret = read(s, pbuf+pos, nleft);
		if (nret  <= 0)
			return nret;
		else
		{
			pos += nret;
			nleft -= nret;
		}
	}
	return pos;
}

#define BRDCST_PORT 15000
int gUdpBrdCstSocket;

typedef enum
{
	BRDCST_CMD_GetDevInfo        = 0x50,
	BRDCST_CMD_SetIpInfo         = 0x51,
	BRDCST_CMD_SetFtpUpdFwInfo   = 0x52,
	BRDCST_CMD_BackToPassvieMode = 0x53,
	BRDCST_CMD_TAGLIST           = 0x54
} BRDCST_CMD_CODE;

BRDCST_DevInfo gBrdCstDevInfo;
uint32 gWorkState = 0;
int gIsBrdHnadlerRun = 0;

int brdcst_conf_init(int sn)
{
	int blkmode = O_NONBLOCK;
	if (socket(sn, Sn_MR_UDP, BRDCST_PORT, 0x00) < 0)
		return -1;
	gUdpBrdCstSocket = sn;
	ioctl(sn, COMMON_INTERFACE_SET_ISBLOCK, &blkmode);
	
	memset(&gBrdCstDevInfo, 0, sizeof(gBrdCstDevInfo));
	memcpy(&gBrdCstDevInfo.network, &gNetConf, sizeof(gNetConf));
#if IS_RTOS2_SUPPORT
	firmware_version(gBrdCstDevInfo.bdfwver);
	gIsBrdHnadlerRun = 1;
#endif
	return 0;
}

void brdcst_conf_handler(void)
{
	extern uint8_t udp_tag_update;
	uint8  brdmsgbuf[210];
	uint8  brdrespbuf[100];
	uint8  remoteip[4];
	uint16 remoteport;
	int pos  = 0;
	int rpos = 0;
	uint8 cmdcode;
	int datalen;
	uint8 macaddr[6];
	uint8 full0mac[6] = {0};
	int isreboot = 1;
	udp_tag_update=0;
	int tmplen;
	uint8 brdaddr[] = {255,255,255,255};
	
	int len = getSn_RX_RSR(gUdpBrdCstSocket);
	if (len > 0)
	{
		len = recvfrom(gUdpBrdCstSocket, brdmsgbuf, 210, remoteip, &remoteport);
		if (brdmsgbuf[pos++] != 0xEE)
			return;
		if (brdmsgbuf[pos++] != 0x06)
			return;
		datalen = GetNumU16(brdmsgbuf+pos);
		pos += 2;
		cmdcode = brdmsgbuf[pos++];
		pos++;
		memcpy(macaddr, brdmsgbuf+pos, 6);
		pos += 6;
		if (memcmp(full0mac, macaddr, 6) == 0 || 
			memcmp(gBrdCstDevInfo.network.mac, macaddr, 6) == 0)
		{
			brdrespbuf[rpos++] = 0xff;
			brdrespbuf[rpos++] = 0x06;
			rpos += 2;
			brdrespbuf[rpos++] = cmdcode;
			brdrespbuf[rpos++] = 0x00;
			SetNumU32(brdrespbuf+rpos, 0);
			rpos += 4;
			memcpy(brdrespbuf+rpos, gBrdCstDevInfo.network.mac, 6);
			rpos += 6;
			switch(cmdcode)
			{
				case BRDCST_CMD_TAGLIST:
					{
				   udp_tag_update=1;
				   isreboot=0;
				   break;
				   }
				case BRDCST_CMD_GetDevInfo:			
					if (memcmp(gBrdCstDevInfo.network.ip, full0mac, 4) == 0)		
					{
						brdrespbuf[rpos++] = 0x01;
						wiz_NetInfo info;
						wizchip_getnetinfo(&info);
						memcpy(brdrespbuf+rpos, info.ip, 4);
						rpos += 4;
						memcpy(brdrespbuf+rpos, info.sn, 4);
						rpos += 4;
						memcpy(brdrespbuf+rpos, info.gw, 4);
						rpos += 4;
						memcpy(brdrespbuf+rpos, info.dns, 4);
						rpos += 4;						
					}
					else
					{
						brdrespbuf[rpos++] = 0x00;
						memcpy(brdrespbuf+rpos, gBrdCstDevInfo.network.ip, 4);
						rpos += 4;
						memcpy(brdrespbuf+rpos, gBrdCstDevInfo.network.subnetMask, 4);
						rpos += 4;
						memcpy(brdrespbuf+rpos, gBrdCstDevInfo.network.gatewayIP, 4);
						rpos += 4;
						memcpy(brdrespbuf+rpos, gBrdCstDevInfo.network.dnsServer, 4);
						rpos += 4;
					}
					SetNumU16(brdrespbuf+rpos, gBrdCstDevInfo.network.listenPort);
					rpos += 2;
					brdrespbuf[rpos++] = 0x01;
					memcpy(brdrespbuf+rpos, gBrdCstDevInfo.modtype, 2);
					rpos += 2;
					memcpy(brdrespbuf+rpos, gBrdCstDevInfo.bdfwver, 4);
					rpos += 4;
					memcpy(brdrespbuf+rpos, gBrdCstDevInfo.modfwver, 4);
					rpos += 4;
					brdrespbuf[rpos++] = gBrdCstDevInfo.workmode;
					SetNumU32(brdrespbuf+6, gWorkState);			
					isreboot = 0;
					break;
				case BRDCST_CMD_SetIpInfo:		
					if (memcmp(full0mac, macaddr, 6) == 0 || datalen != 24)
						return;
					else
					{
						networkParaConfig netcfg;
						memcpy(netcfg.ip, brdmsgbuf+pos, 4);
						pos += 4;
						memcpy(netcfg.subnetMask, brdmsgbuf+pos, 4);
						pos += 4;
						memcpy(netcfg.gatewayIP, brdmsgbuf+pos, 4);
						pos += 4;						
						memcpy(netcfg.dnsServer, brdmsgbuf+pos, 4);
						pos += 4;
						memcpy(netcfg.mac, brdmsgbuf+pos, 6);
						pos += 6;
						netcfg.listenPort = GetNumU16(brdmsgbuf+pos);
						set_network_config(&netcfg);						
					}
					break;
				case BRDCST_CMD_SetFtpUpdFwInfo:
					{
						TRACE("case BRDCST_CMD_SetFtpUpdFwInfo:\n");
						BtParams_ST btparas;
						btparas.updateflag = 'V';
						btparas.updatemode = FwUpdateMode_ByFtp_FmEth;
						tmplen = brdmsgbuf[pos++];
						if (tmplen > BTFWUPD_FTP_USER_BUFLEN)
						{
							SetNumU32(brdrespbuf+6, 7);
							break;
						}
						memcpy(btparas.ftpuser, brdmsgbuf+pos, tmplen);
						pos += tmplen;
						btparas.ftpuser[tmplen] = 0;
						
						tmplen = brdmsgbuf[pos++];
						if (tmplen > BTFWUPD_FTP_PASSWORD_BUFLEN)
						{
							SetNumU32(brdrespbuf+6, 7);
							break;
						}
						memcpy(btparas.ftppassword, brdmsgbuf+pos, tmplen);
						pos += tmplen;
						btparas.ftppassword[tmplen] = 0;

						tmplen = brdmsgbuf[pos++];
						if (tmplen > BTFWUPD_FTP_SERADDR_BUFLEN)
						{
							SetNumU32(brdrespbuf+6, 7);
							break;
						}
						memcpy(btparas.ftpaddr, brdmsgbuf+pos, tmplen);
						pos += tmplen;
						btparas.ftpaddr[tmplen] = 0;
						
						tmplen = brdmsgbuf[pos++];						
						if (tmplen > BTFWUPD_FTP_FILENAME_BUFLEN)
						{
							SetNumU32(brdrespbuf+6, 7);
							break;
						}
						memcpy(btparas.filename, brdmsgbuf+pos, tmplen);
						pos += tmplen;
						btparas.filename[tmplen] = 0;
						TRACE("filelen:%d, str:%s\n", tmplen, btparas.filename);
						setBtParams(&btparas);
					}
					break;
				case BRDCST_CMD_BackToPassvieMode:
					erase_active_mode_params();
					break;
				default:
					break;
			}
			if(udp_tag_update==0)
			{
			SetNumU16(brdrespbuf+2, rpos-16);
			sendto(gUdpBrdCstSocket, brdrespbuf, rpos, brdaddr, remoteport);
			}	
			if (isreboot)
			{
				sleep_ms(200);
				system_reset();
			}
		}
	}
}


int UDP_send(uint8_t * buf, uint16_t len)
{
   uint8_t brdaddr[] = {255,255,255,255};
	 uint16_t sentlen; 
   sentlen=sendto(gUdpBrdCstSocket, buf, len, brdaddr, BRDCST_PORT);
	 return sentlen;
}	





