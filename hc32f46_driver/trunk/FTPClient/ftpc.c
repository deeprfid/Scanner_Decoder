#include "ftpc.h"
#include "hc32f46_driver.h"

un_l2cval remote_ip;
uint16_t  remote_port;
//un_l2cval local_ip;
uint16_t  local_port;
uint8_t connect_state_control_ftpc = 0;
uint8_t connect_state_data_ftpc = 0;
uint8_t gModeActivePassiveflag = 0;
uint8_t FTP_destip[4];	// For FTP client examples; destination network info
uint16_t FTP_destport = 21;						// For FTP client examples; destination network info
uint8_t gMenuStart = 0;
uint8_t gDataSockReady = 0;
uint8_t gDataPutGetStart = 0;
static uint8_t gMsgBuf[50]={0,};

struct ftpc ftpc;
struct Command Command;

char *dummyKeyboardInput;
uint8_t gTransferFin = 0;

int gKInputPos = 0;
uint8_t Board_UARTGetCharBlocking()
{	
	if (gKInputPos == strlen(dummyKeyboardInput))
		while(1);
	return dummyKeyboardInput[gKInputPos++];
	
//	return chr;
}
ftpDataCallback gDataRcb = NULL;
ftpInitCallback gInitcb = NULL;
unsigned long long lastGetDataTime;
#define MaxNoDataArriveTimeout 10000

void ftpc_destory(void)
{
	uint8_t iomode = SOCK_IO_NONBLOCK;
	free_hexp(dummyKeyboardInput);
	ctlsocket(DATA_SOCK, CS_SET_IOMODE, &iomode);
	disconnect(DATA_SOCK);
	close(DATA_SOCK);
	ctlsocket(CTRL_SOCK, CS_SET_IOMODE, &iomode);
	disconnect(CTRL_SOCK);
	close(CTRL_SOCK);
}

void ftpc_init(uint8_t * ser_ip, char *user, 
	char *password, char *filepath, uint16 port, 
		ftpInitCallback initcb, ftpDataCallback datacb)
{
	ftpc.dsock_mode = PASSIVE_MODE;
	ftpc.type = IMAGE_TYPE;
	gTransferFin = 0;
	gMenuStart = 0;
	gKInputPos = 0;
	
	FTP_destip[0] = ser_ip[0];
	FTP_destip[1] = ser_ip[1];
	FTP_destip[2] = ser_ip[2];
	FTP_destip[3] = ser_ip[3];
	
	dummyKeyboardInput = malloc_hexp(strlen(user)+strlen(password)+strlen(filepath)+10);
	sprintf(dummyKeyboardInput, "%s\r%s\r6%s\r", user, password, filepath);
//	printf("dummyKeyboardInput:%s\n", dummyKeyboardInput);
//	local_ip.cVal[0] = src_ip[0];
//	local_ip.cVal[1] = src_ip[1];
//	local_ip.cVal[2] = src_ip[2];
//	local_ip.cVal[3] = src_ip[3];
	local_port = 35000;
	strcpy(ftpc.workingdir, "/");
	socket(CTRL_SOCK, Sn_MR_TCP, local_port, 0x0);
	gDataRcb = datacb;
	gInitcb = initcb;
	FTP_destport = port;
}
int ftpc_run(uint8_t * dbuf)
{
#ifndef Need_UARTGetCharBlocking_func
	uint16_t size = 0;
	long ret = 0;
	uint32_t  recv_byte;
	uint32_t blocklen;
	uint32_t remain_filesize;
	uint32_t remain_datasize;
	uint8_t msg_c;
	char dat[50]={0,};
//	uint32_t totalSize = 0, availableSize = 0;

    switch(getSn_SR(CTRL_SOCK))
    {
    	case SOCK_ESTABLISHED :
			if (gTransferFin == 1)
				return R_226;
    		if(!connect_state_control_ftpc){
    			TRACE("%d:FTP Connected\r\n", CTRL_SOCK);
    			strcpy(ftpc.workingdir, "/");
    			connect_state_control_ftpc = 1;
    		}
    		if(gMenuStart){
				gMenuStart = 0;
				TRACE("\r\n----------------------------------------\r\n");
				TRACE("Press menu key\r\n");
				TRACE("----------------------------------------\r\n");
				TRACE("1> View FTP Server Directory\r\n");
				TRACE("2> View My Directory\r\n");
				TRACE("3> Sets the type of file to be transferred. Current state : %s\r\n", (ftpc.type==ASCII_TYPE)?"Ascii":"Binary");
				TRACE("4> Sets Data Connection. Current state : %s\r\n", (ftpc.dsock_mode==ACTIVE_MODE)?"Active":"Passive");
				TRACE("5> Put File to Server\r\n");
				TRACE("6> Get File from Server\r\n");
#if defined(F_FILESYSTEM)
				TRACE("7> Delete My File\r\n");
#endif
				TRACE("----------------------------------------\r\n");
				while(1){
					msg_c=ftp_getc();
					if(msg_c=='1'){
						if(ftpc.dsock_mode==PASSIVE_MODE){
							sprintf(dat,"PASV\r\n");
							send(CTRL_SOCK, (uint8_t *)dat, strlen(dat));
							Command.First = f_dir;
							break;
						}
						else{
							wiz_NetInfo gWIZNETINFO;
							ctlnetwork(CN_GET_NETINFO, (void*) &gWIZNETINFO);
							sprintf(dat,"PORT %d,%d,%d,%d,%d,%d\r\n", gWIZNETINFO.ip[0], gWIZNETINFO.ip[1], gWIZNETINFO.ip[2], gWIZNETINFO.ip[3], (uint8_t)(local_port>>8), (uint8_t)(local_port&0x00ff));
							send(CTRL_SOCK, (uint8_t *)dat, strlen(dat));
							Command.First = f_dir;

							gModeActivePassiveflag = 1;
							break;
						}
//						break;
					}
					else if(msg_c=='5'){
						if(ftpc.dsock_mode==PASSIVE_MODE){
							sprintf(dat,"PASV\r\n");
							send(CTRL_SOCK, (uint8_t *)dat, strlen(dat));
							Command.First = f_put;
							break;
						}
						else{
							wiz_NetInfo gWIZNETINFO;
							ctlnetwork(CN_GET_NETINFO, (void*) &gWIZNETINFO);
							sprintf(dat,"PORT %d,%d,%d,%d,%d,%d\r\n", gWIZNETINFO.ip[0], gWIZNETINFO.ip[1], gWIZNETINFO.ip[2], gWIZNETINFO.ip[3], (uint8_t)(local_port>>8), (uint8_t)(local_port&0x00ff));
							send(CTRL_SOCK, (uint8_t *)dat, strlen(dat));
							Command.First = f_put;

							gModeActivePassiveflag = 1;
							break;
						}
					}
					else if(msg_c=='6'){
						if(ftpc.dsock_mode==PASSIVE_MODE){
							sprintf(dat,"PASV\r\n");
							send(CTRL_SOCK, (uint8_t *)dat, strlen(dat));
							Command.First = f_get;
							break;
						}
						else{
							wiz_NetInfo gWIZNETINFO;
							ctlnetwork(CN_GET_NETINFO, (void*) &gWIZNETINFO);
							sprintf(dat,"PORT %d,%d,%d,%d,%d,%d\r\n", gWIZNETINFO.ip[0], gWIZNETINFO.ip[1], gWIZNETINFO.ip[2], gWIZNETINFO.ip[3], (uint8_t)(local_port>>8), (uint8_t)(local_port&0x00ff));
							send(CTRL_SOCK, (uint8_t *)dat, strlen(dat));
							Command.First = f_get;

							gModeActivePassiveflag = 1;
							break;
						}
					}
					else if(msg_c=='2'){
#if defined(F_FILESYSTEM)
						scan_files(ftpc.workingdir, dbuf, (int *)&size);
						TRACE("\r\n%s\r\n", dbuf);
#else
						if (strncmp(ftpc.workingdir, "/$Recycle.Bin", sizeof("/$Recycle.Bin")) != 0)
							size = sprintf((char *)dbuf, "drwxr-xr-x 1 ftp ftp 0 Dec 31 2014 $Recycle.Bin\r\n-rwxr-xr-x 1 ftp ftp 512 Dec 31 2014 test.txt\r\n");
						TRACE("\r\n%s\r\n", dbuf);
#endif
						gMenuStart = 1;
						break;
					}
					else if(msg_c=='3'){
						TRACE("1> ASCII\r\n");
						TRACE("2> BINARY\r\n");
						while(1){
							msg_c=ftp_getc();
							if(msg_c=='1'){
								sprintf(dat,"TYPE %c\r\n", TransferAscii);
								ftpc.type = ASCII_TYPE;
								send(CTRL_SOCK, (uint8_t *)dat, strlen(dat));
								break;
							}
							else if(msg_c=='2'){
								sprintf(dat,"TYPE %c\r\n", TransferBinary);
								ftpc.type = IMAGE_TYPE;
								send(CTRL_SOCK, (uint8_t *)dat, strlen(dat));
								break;
							}
							else{
								TRACE("\r\nRetry...\r\n");
							}
						}
						break;
					}
					else if(msg_c=='4'){
						TRACE("1> ACTIVE\r\n");
						TRACE("2> PASSIVE\r\n");
						while(1){
							msg_c=ftp_getc();
							if(msg_c=='1'){
								ftpc.dsock_mode=ACTIVE_MODE;
								break;
							}
							else if(msg_c=='2'){
								ftpc.dsock_mode=PASSIVE_MODE;
								break;
							}
							else{
								TRACE("\r\nRetry...\r\n");
							}
						}
						gMenuStart = 1;
						break;
					}
#if defined(F_FILESYSTEM)
					else if(msg_c=='7'){
						TRACE(">del filename?");
						sprintf(ftpc.filename, "/%s\r\n", User_Keyboard_MSG());
						if (f_unlink((const char *)ftpc.filename) != 0){
							TRACE("\r\nCould not delete.\r\n");
						}
						else{
							TRACE("\r\nDeleted.\r\n");
						}
						gMenuStart = 1;
						break;
					}
#endif
					else{
						TRACE("\r\nRetry...\r\n");
					}
				}
			}
			if(gDataSockReady){
				gDataSockReady = 0;
				switch(Command.First){
					case f_dir:
						sprintf(dat,"LIST\r\n");
						send(CTRL_SOCK, (uint8_t *)dat, strlen(dat));
						break;
					case f_put:
						TRACE(">put file name?");
						sprintf(dat,"STOR %s\r\n", User_Keyboard_MSG());
						send(CTRL_SOCK, (uint8_t *)dat, strlen(dat));
						break;
					case f_get:
						TRACE(">get file name?");
						sprintf(dat,"RETR %s\r\n", User_Keyboard_MSG());						
						send(CTRL_SOCK, (uint8_t *)dat, strlen(dat));
						break;
					default:
						TRACE("Command.First = default\r\n");
						break;
				}
			}
    		if((size = getSn_RX_RSR(CTRL_SOCK)) > 0){ // Don't need to check SOCKERR_BUSY because it doesn't not occur.
				int retproc_ftpc;
    			memset(dbuf, 0, _MAX_SS);
    			if(size > _MAX_SS) size = _MAX_SS - 1;
    			ret = recv(CTRL_SOCK,dbuf,size);
    			dbuf[ret] = '\0';
    			if(ret != size)
    			{
    				if(ret==SOCK_BUSY) return 0;
    				if(ret < 0){
    					TRACE("%d:recv() error:%ld\r\n",CTRL_SOCK,ret);
    					close(CTRL_SOCK);
    					return ret;
    				}
    			}
    			TRACE("Rcvd Command: %s\r\n", dbuf);
				retproc_ftpc = proc_ftpc((char *)dbuf);
    			if (retproc_ftpc == 2)
					return -12346;
				else if (retproc_ftpc == 3)
					return -12347;
    		}
    		break;
    	case SOCK_CLOSE_WAIT :
    		TRACE("%d:CloseWait\r\n",CTRL_SOCK);
    		if((ret=disconnect(CTRL_SOCK)) != SOCK_OK) return ret;
    		TRACE("%d:Closed\r\n",CTRL_SOCK);
    		break;
    	case SOCK_CLOSED :
    		TRACE("%d:FTPStart\r\n",CTRL_SOCK);
    		if((ret=socket(CTRL_SOCK, Sn_MR_TCP, FTP_destport, 0x0)) != CTRL_SOCK){
    			TRACE("%d:socket() error:%ld\r\n", CTRL_SOCK, ret);
    			close(CTRL_SOCK);
    			return ret;
    		}
    		break;
    	case SOCK_INIT :
    		TRACE("%d:Opened\r\n",CTRL_SOCK);
			if((ret = connect(CTRL_SOCK, FTP_destip, FTP_destport)) != SOCK_OK){
				TRACE("%d:Connect error:%d\r\n",CTRL_SOCK, ret);
				return ret;
			}
			connect_state_control_ftpc = 0;
			TRACE("%d:Connectting...\r\n",CTRL_SOCK);
			break;
    	default :
    		break;
    }

    switch(getSn_SR(DATA_SOCK)){
    	case SOCK_ESTABLISHED :
			TRACE("DATA_SOCK SOCK_ESTABLISHED\n");
    		if(!connect_state_data_ftpc){
    			TRACE("%d:FTP Data socket Connected\r\n", DATA_SOCK);
    			connect_state_data_ftpc = 1;
    		}
			if(gDataPutGetStart){
				switch(Command.Second){
				case s_dir:
					TRACE("dir waiting...\r\n");
					if((size = getSn_RX_RSR(DATA_SOCK)) > 0){ // Don't need to check SOCKERR_BUSY because it doesn't not occur.
						TRACE("ok\r\n");
						memset(dbuf, 0, _MAX_SS);
						if(size > _MAX_SS) size = _MAX_SS - 1;
						ret = recv(DATA_SOCK,dbuf,size);
						dbuf[ret] = '\0';
						if(ret != size){
							if(ret==SOCK_BUSY) return 0;
							if(ret < 0){
								TRACE("%d:recv() error:%ld\r\n",CTRL_SOCK,ret);
								close(DATA_SOCK);
								return ret;
							}
						}
						TRACE("Rcvd Data:\n\r%s\n\r", dbuf);
						gDataPutGetStart = 0;
						Command.Second = s_nocmd;
					}
					break;
				case s_put:
					TRACE("put waiting...\r\n");
					if(strlen(ftpc.workingdir) == 1)
						sprintf(ftpc.filename, "/%s", (uint8_t *)gMsgBuf);
					else
						sprintf(ftpc.filename, "%s/%s", ftpc.workingdir, (uint8_t *)gMsgBuf);
#if defined(F_FILESYSTEM)
					ftpc.fr = f_open(&(ftpc.fil), (const char *)ftpc.filename, FA_READ);
					if(ftpc.fr == FR_OK){
						remain_filesize = ftpc.fil.fsize;
						TRACE("f_open return FR_OK\r\n");
						do{
							memset(dbuf, 0, _MAX_SS);
							if(remain_filesize > _MAX_SS)
								send_byte = _MAX_SS;
							else
								send_byte = remain_filesize;
							ftpc.fr = f_read(&(ftpc.fil), (void *)dbuf, send_byte , (UINT *)&blocklen);
							if(ftpc.fr != FR_OK){
								break;
							}
							TRACE("#");
							send(DATA_SOCK, dbuf, blocklen);
							remain_filesize -= blocklen;
						}while(remain_filesize != 0);
						TRACE("\r\nFile read finished\r\n");
						ftpc.fr = f_close(&(ftpc.fil));
					}
					else{
						TRACE("File Open Error: %d\r\n", ftpc.fr);
						ftpc.fr = f_close(&(ftpc.fil));
					}
#else
					remain_filesize = strlen(ftpc.filename);
					do{
						memset(dbuf, 0, _MAX_SS);
						blocklen = sprintf((char *)dbuf, "%s", ftpc.filename);
//						printf("00000 ########## dbuf:%s\r\n", dbuf);
						send(DATA_SOCK, dbuf, blocklen);
						remain_filesize -= blocklen;
					}while(remain_filesize != 0);
#endif
					gDataPutGetStart = 0;
					Command.Second = s_nocmd;
					disconnect(DATA_SOCK);
					break;
				case s_get:
					TRACE("get waiting...\r\n");
					if(strlen(ftpc.workingdir) == 1)
						sprintf(ftpc.filename, "/%s", (uint8_t *)gMsgBuf);
					else
						sprintf(ftpc.filename, "%s/%s", ftpc.workingdir, (uint8_t *)gMsgBuf);
#if defined(F_FILESYSTEM)
					ftpc.fr = f_open(&(ftpc.fil), (const char *)ftpc.filename, FA_CREATE_ALWAYS | FA_WRITE);
					if(ftpc.fr == FR_OK){
						TRACE("f_open return FR_OK\r\n");
						while(1){
							if((remain_datasize = getSn_RX_RSR(DATA_SOCK)) > 0){
								while(1){
									memset(dbuf, 0, _MAX_SS);
									if(remain_datasize > _MAX_SS)	recv_byte = _MAX_SS;
									else	recv_byte = remain_datasize;
									ret = recv(DATA_SOCK, dbuf, recv_byte);
									ftpc.fr = f_write(&(ftpc.fil), (const void *)dbuf, (UINT)ret, (UINT *)&blocklen);
									remain_datasize -= blocklen;
									if(ftpc.fr != FR_OK){
										TRACE("f_write failed\r\n");
										break;
									}
									if(remain_datasize <= 0)	break;
								}
								if(ftpc.fr != FR_OK){
									TRACE("f_write failed\r\n");
									break;
								}
								TRACE("#");
							}
							else{
								if(getSn_SR(DATA_SOCK) != SOCK_ESTABLISHED)	break;
							}
						}
						TRACE("\r\nFile write finished\r\n");
						ftpc.fr = f_close(&(ftpc.fil));
						gDataPutGetStart = 0;
					}else{
						TRACE("File Open Error: %d\r\n", ftpc.fr);
					}
#else
					lastGetDataTime = getSysTick();
//					printf("00000000 remain_datasize:%d\n", remain_datasize);
					while(1){
						if((remain_datasize = getSn_RX_RSR(DATA_SOCK)) > 0){
							lastGetDataTime = getSysTick();
							TRACE("lastGetDataTime:%lld, remain_datasize:%d\n", lastGetDataTime, remain_datasize);
							while(1){
								memset(dbuf, 0, _MAX_SS);
								if(remain_datasize > _MAX_SS)
									recv_byte = _MAX_SS;
								else
									recv_byte = remain_datasize;
								ret = recv(DATA_SOCK, dbuf, recv_byte);
//								printf("11111 ########## dbuf:%s\r\n", dbuf);
								remain_datasize -= ret;
								if (ret > 0)
								{
									if (gDataRcb != NULL)
									{
										int retcb = gDataRcb(dbuf, ret);
										if (retcb != 0)
										{
											TRACE("if (retcb != 0)if (retcb != 0)\n");
											return -12345;
										}
									}
								}
								if(remain_datasize <= 0)
									break;
							}
						}else{
							uint64 nowlltm = getSysTick();
							if (nowlltm > lastGetDataTime)
							{
								if (nowlltm - lastGetDataTime > MaxNoDataArriveTimeout)
								{
									TRACE("MaxNoDataArriveTimeout nowlltm:%lld, lastGetDataTime:%lld\n", 
										nowlltm, lastGetDataTime);
									return -12345;
								}
							}
							else
								TRACE("-------------- nowlltm:%lld, lastGetDataTime:%lld\n", 
										nowlltm, lastGetDataTime);
							if(getSn_SR(DATA_SOCK) != SOCK_ESTABLISHED)
								break;
						}
					}
					gDataPutGetStart = 0;
					Command.Second = s_nocmd;
#endif
					break;
				default:
					TRACE("Command.Second = default\r\n");
					break;
				}
			}
    		break;
   		case SOCK_CLOSE_WAIT :
   			TRACE("%d:CloseWait\r\n",DATA_SOCK);
			if((ret=disconnect(DATA_SOCK)) != SOCK_OK) return ret;
			TRACE("%d:Closed\r\n",DATA_SOCK);
   			break;
   		case SOCK_CLOSED :
				TRACE("DATA_SOCK SOCK_CLOSED ftpc.dsock_state:%d\n", ftpc.dsock_state);
   			if(ftpc.dsock_state == DATASOCK_READY){
   				if(ftpc.dsock_mode == PASSIVE_MODE){
   					TRACE("%d:FTPDataStart, port : %d\r\n",DATA_SOCK, local_port);
   					if((ret=socket(DATA_SOCK, Sn_MR_TCP, local_port, 0x0)) != DATA_SOCK){
   						TRACE("%d:socket() error:%ld\r\n", DATA_SOCK, ret);
   						close(DATA_SOCK);
   						return ret;
   					}
   					local_port++;
   					if(local_port > 50000)
   						local_port = 35000;
   				}else{
   					TRACE("%d:FTPDataStart, port : %d\r\n",DATA_SOCK, local_port);
   					if((ret=socket(DATA_SOCK, Sn_MR_TCP, local_port, 0x0)) != DATA_SOCK){
   						TRACE("%d:socket() error:%ld\r\n", DATA_SOCK, ret);
   						close(DATA_SOCK);
   						return ret;
   					}
   					local_port++;
   					if(local_port > 50000)
   						local_port = 35000;
   				}
   				ftpc.dsock_state = DATASOCK_START;
   			}
   			break;

   		case SOCK_INIT :
   			TRACE("%d:Opened\r\n",DATA_SOCK);
   			if(ftpc.dsock_mode == ACTIVE_MODE){
   				if( (ret = listen(DATA_SOCK)) != SOCK_OK){
   					TRACE("%d:Listen error\r\n",DATA_SOCK);
   					return ret;
   				}
   				gDataSockReady = 1;
   				TRACE("%d:Listen ok\r\n",DATA_SOCK);
   			}else{
   				if((ret = connect(DATA_SOCK, remote_ip.cVal, remote_port)) != SOCK_OK){
   					TRACE("%d:Connect error\r\n", DATA_SOCK);
   					return -12345;
   				}
   				gDataSockReady = 1;
   			}
   			connect_state_data_ftpc = 0;
   			break;
   		default :
				TRACE("DATA_SOCK default\n");
   			break;
    }
#endif
    return 0;
}

char proc_ftpc(char * buf)
{
	uint16_t Responses;
	char dat[30]={0,};

	Responses =(buf[0]-'0')*100+(buf[1]-'0')*10+(buf[2]-'0');

	switch(Responses){
		case R_220:	/* Service ready for new user. */
			if (gInitcb != NULL)
				gInitcb();
			gKInputPos = 0;
							
			TRACE("\r\nInput your User ID > ");
			sprintf(dat,"USER %s\r\n", User_Keyboard_MSG());
			TRACE("\r\n");
			send(CTRL_SOCK, (uint8_t *)dat, strlen(dat));
			break;

		case R_331:	/* User name okay, need password. */
			TRACE("\r\nInput your Password > ");
			sprintf(dat,"PASS %s\r\n", User_Keyboard_MSG());
			TRACE("\r\n");
			send(CTRL_SOCK, (uint8_t *)dat, strlen(dat));
			break;
		case R_230:	/* User logged in, proceed */
			TRACE("\r\nUser logged in, proceed\r\n");

//			sprintf(dat,"TYPE %c\r\n", TransferAscii);
			sprintf(dat,"TYPE %c\r\n", TransferBinary);
			ftpc.type = IMAGE_TYPE;
			send(CTRL_SOCK, (uint8_t *)dat, strlen(dat));
			break;
		case R_200:
			if((ftpc.dsock_mode==ACTIVE_MODE)&&gModeActivePassiveflag){
				ftpc.dsock_state = DATASOCK_READY;
				gModeActivePassiveflag = 0;
			}
			else{
				gMenuStart = 1;
			}
			break;
		case R_150:
			switch(Command.First){
			case f_dir:
				Command.First = f_nocmd;
				Command.Second = s_dir;
				gDataPutGetStart = 1;
				break;
			case f_get:
				Command.First = f_nocmd;
				Command.Second = s_get;
				gDataPutGetStart = 1;
				break;
			case f_put:
				Command.First = f_nocmd;
				Command.Second = s_put;
				gDataPutGetStart = 1;
				break;
			default :
				TRACE("Command.First = default\r\n");
				break;
			}
			break;
		case R_226:
			gMenuStart = 1;
			gTransferFin = 1;
			break;
		case R_227:
			if (pportc(buf) == -1){
				TRACE("Bad port syntax\r\n");
			}
			else{
				TRACE("Go Open Data Sock...\r\n ");
				ftpc.dsock_mode = PASSIVE_MODE;
				ftpc.dsock_state = DATASOCK_READY;
			}
			break;
		case R_530:
			return 2;
		case R_550:
			return 3;
		default:
			TRACE("\r\nDefault Status = %d\r\n",(uint16_t)Responses);
			gDataSockReady = 1;
			break;
		}
	return 1;
}
int pportc(char * arg)
{
	int i;
	char* tok=0;
	strtok(arg,"(");
	for (i = 0; i < 4; i++)
	{
		if(i==0) tok = strtok(NULL,",\r\n");
		else	 tok = strtok(NULL,",");
		remote_ip.cVal[i] = (uint8_t)atoi(tok);
		if (!tok){
			TRACE("bad pport : %s\r\n", arg);
			return -1;
		}
	}
	remote_port = 0;
	for (i = 0; i < 2; i++){
		tok = strtok(NULL,",\r\n");
		remote_port <<= 8;
		remote_port += atoi(tok);
		if (!tok){
			TRACE("bad pport : %s\r\n", arg);
			return -1;
		}
	}
	if (remote_ip.cVal[0] == 127 && 
		remote_ip.cVal[1] == 0 && 
		remote_ip.cVal[2] == 0 && 
		remote_ip.cVal[3] == 1)
	{
		TRACE("change 127.0.0.1 to destip\n");
		memcpy(remote_ip.cVal, FTP_destip, 4);
	}
	TRACE("ip : %d.%d.%d.%d, port : %d\r\n", remote_ip.cVal[0], remote_ip.cVal[1], remote_ip.cVal[2], remote_ip.cVal[3], remote_port);
	return 0;
}
uint8_t* User_Keyboard_MSG()
{
	uint8_t i=0;
	do{
		gMsgBuf[i] = ftp_getc();
		i++;
	}while(gMsgBuf[i-1]!=0x0d);
	gMsgBuf[i-1]=0;
//	printf("00000000000000000000000000000000 gMsgBuf:%s\n", gMsgBuf);
	return gMsgBuf;
}
