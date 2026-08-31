#include <stdlib.h>
#include "hc32f46_driver.h"
#include "http_parser.h"
#include "http_callback.h"
#include "app_conf.h"
#include <string.h>
#include "reader_msg.h"

int gHttpRespBodyLen;
int gHttpParseErr;

#define HttpRespJsonBufLen 256
char *gHttpRespJsonBuf;
int gIsFinHttpParse;

int gMbedNetFd = 0;
int gIsTlsConn = 0;

int http_OnBodyCallback(http_parser *parser, 
	const char *at, size_t length)
{
	if (gHttpRespBodyLen+length > HttpRespJsonBufLen -1)
		gHttpParseErr = 400;
	else
	{
		memcpy(gHttpRespJsonBuf+gHttpRespBodyLen, at, length);
		gHttpRespBodyLen += length;
	}
	return 0;
}

int http_OnMessageCompleteCallback(http_parser *parser)
{
	gHttpRespJsonBuf[gHttpRespBodyLen] = 0;

	gIsFinHttpParse = 1;
	return 0;
}

int http_OnHeadersCompleteCallback(http_parser *parser)
{
	gHttpRespBodyLen = 0;

	return 0;
}

int gAbsPathpos;
extern ReaderRunTimeSettings_ST *gRtSetting;
int Write_N_NoBlk(int fd, unsigned char *buf, int len, int reconn);

http_parser_settings *gPHttpParseSet;
http_parser *gPHttpParser;
	
void init_http_fn(void)
{
	gPHttpParseSet = malloc_hexp(sizeof(http_parser_settings));
	gPHttpParser = malloc_hexp(sizeof(http_parser));
	http_parser_settings_init(gPHttpParseSet);

	gPHttpParseSet->on_message_complete = http_OnMessageCompleteCallback;
	gPHttpParseSet->on_body = http_OnBodyCallback;
	
	gHttpRespJsonBuf = malloc_hexp(HttpRespJsonBufLen);
}
char gHostName[80];
int url_get_domain(char *url, char *domain, 
	unsigned short *port, int *abrpos, int *ishttps)
{
	char *urlpos;
	char *tmp;
	int len;
	char numbuf[10];
	int dmstart;
	
	strncpy(numbuf, url, 5);
	toupper_arm(numbuf);
	if (strstr(numbuf, "HTTPS") != NULL)
	{
		dmstart = 8;
		*ishttps = 1;
	}
	else
	{
		dmstart = 7;
		*ishttps = 0;
	}
	
	urlpos = strstr(url+dmstart, "/");		
	if (urlpos == NULL)
		strcat(url, "/");

	urlpos = strstr(url+7, ":");
	if (urlpos == NULL)
	{
		if (*ishttps == 0)
			*port = 80;
		else
			*port = 443;
		
		urlpos = strstr(url+dmstart, "/");
		len = urlpos-url-dmstart;
		memcpy(domain, url+dmstart, len);
		domain[len] = 0;
		*abrpos = len+dmstart;
	}
	else
	{
		len = urlpos-url-dmstart;
		memcpy(domain, url+dmstart, len);
		domain[len] = 0;
		tmp = strstr(urlpos, "/");

		memcpy(numbuf, urlpos+1, tmp-urlpos-1);
		numbuf[tmp-urlpos-1] = 0;
		*port = atoi_Arm(numbuf);
		*abrpos = tmp - url;
	}
	sprintf(gHostName, "%s:%d", domain, *port);
	TRACE("domain:%s, port:%d, abr:%s\n", domain, *port, url+*abrpos);
	return 0;
}

//char httprespdumpbuf[1024*2];
#define DEBUG_LEVEL 4

static void my_debug(void* ctx, int level,
    const char* file, int line,
    const char* str)
{
    ((void)level);

    TRACE("%s:%04d: %s", file, line, str);
//    fflush((FILE*)ctx);
}

/**
* @brief   ∑µªÿ ±º‰¥¡
* @param   None
* @retval  None
* @warning None
* @example
**/
//struct tm *lcTime;
//time_t startTime;
//    lcTime = localtime (&startTime);
time_t gSysTime_Base = 0;
time_t gAbsTime_Base = 0;
time_t time(time_t *t)
{
//    time_t it;
    if (t) {
		 
		 gSysTime_Base = getSysTick();
		 gAbsTime_Base = *t;
        return *t;
    }
    else
    {
//        startTime = 0;
//        lcTime = localtime (&startTime);
//        it = mktime(lcTime);
//        return it ;
		 return gAbsTime_Base + getSysTick() - gSysTime_Base;
    }
}

int os_get_random(unsigned char *buf, size_t len)
{
    int i, j;
    unsigned long tmp;
 
    for (i = 0; i < ((len + 3) & ~3) / 4; i++) {
        tmp = rand() + (getSysTick() & 0xffffffff);
 
        for (j = 0; j < 4; j++) {
            if ((i * 4 + j) < len) {
                buf[i * 4 + j] = (uint8_t)(tmp >> (j * 8));
            } else {
                break;
            }
        }
    }
 
    return 0;
}

int mbedtls_hardware_poll( void *data, unsigned char *output, size_t len, size_t *olen )
{
    int res = os_get_random(output, len);
    *olen = len;
    return 0;
}

int mbed_net_send( void *ctx, const unsigned char *buf, size_t len )
{
	int ret;
//	printf("--------------usrc216_net_send len:%d\n", len);
	if (gMbedNetFd == COMMON_INTERFACE_SOCKET0)
	{
		ret = write(gMbedNetFd, (uint8 *)buf, len);
		if (ret != len)
		{
			TRACE("write(gMbedNetFd, != len, len:%d, ret:%d\n", len, ret);
			return 0;
		}
		else
			return len;
	}
	else
		return write(gMbedNetFd, buf, len);
//	printf("after usrc216_net_send\n");
	 
}

int  mbed_net_recv_timeout( void *ctx, unsigned char *buf,
                              size_t len, uint32_t timeout )
{
	int ret;
	int timeout_ = timeout;
	ioctl(gMbedNetFd, COMMON_INTERFACE_SET_TIMEOUT, &timeout_);
//	printf("++++++++++mbed_net_recv_timeout  len:%d\n", len);
	ret = read(gMbedNetFd, buf, len);
	if (ret == -2)
		ret = MBEDTLS_ERR_SSL_TIMEOUT;

	return ret;
}

mbedtls_entropy_context *g_entropy = NULL;
mbedtls_ctr_drbg_context *g_ctr_drbg = NULL;
mbedtls_ssl_context *g_ssl_context = NULL;
mbedtls_ssl_config *g_ssl_conf = NULL;

int gMbedtlsInit = 0;
void preinit_mbedtls(void)
{
	g_entropy = malloc_hexp(sizeof(mbedtls_entropy_context));
	g_ctr_drbg = malloc_hexp(sizeof(mbedtls_ctr_drbg_context));
	g_ssl_context = malloc_hexp(sizeof(mbedtls_ssl_context));
	g_ssl_conf = malloc_hexp(sizeof(mbedtls_ssl_config));	
}

int init_mbedtls(void)
{
	const char* pers = "ssl_client1";
	
	if (gMbedtlsInit == 1)
		return 0;
#if defined(MBEDTLS_DEBUG_C)
    mbedtls_debug_set_threshold(DEBUG_LEVEL);
#endif

	mbedtls_platform_set_calloc_free(calloc_hexp, free_hexp);
	mbedtls_ssl_init(g_ssl_context);
	mbedtls_ssl_config_init(g_ssl_conf);
	mbedtls_ctr_drbg_init(g_ctr_drbg);
//	TRACE("\n  . Seeding the random number generator...");
	mbedtls_entropy_init(g_entropy);
   if (mbedtls_ctr_drbg_seed(g_ctr_drbg, mbedtls_entropy_func, g_entropy,
		(const unsigned char*)pers, strlen(pers)) != 0) 
	{
		TRACE(" failed\n  ! mbedtls_ctr_drbg_seed returned\n");
		return -1;
	}
//	TRACE(" ok\n");
//	TRACE("  . Setting up the SSL/TLS structure...");

	if (mbedtls_ssl_config_defaults(g_ssl_conf, MBEDTLS_SSL_IS_CLIENT,
		MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0)
	{
		TRACE(" failed\n  ! mbedtls_ssl_config_defaults returned\n\n");
      return -1;
   }

//    TRACE(" ok\n");

	 mbedtls_ssl_conf_read_timeout(g_ssl_conf, 8000);
    mbedtls_ssl_conf_authmode(g_ssl_conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(g_ssl_conf, mbedtls_ctr_drbg_random, g_ctr_drbg);
    mbedtls_ssl_conf_dbg(g_ssl_conf, my_debug, stdout);

    if (mbedtls_ssl_setup(g_ssl_context, g_ssl_conf) != 0)
	 {
		 TRACE(" failed\n  ! mbedtls_ssl_setup returned \n\n");
       return -1;
    }
	 
//	 mbedtls_ssl_set_hostname(&ssl, SERVER_NAME);
	 mbedtls_ssl_set_bio(g_ssl_context, NULL, mbed_net_send, NULL, mbed_net_recv_timeout);
	 gMbedtlsInit = 1;
	 return 0;
}

void deinit_mbedtls(void)
{
	if (gMbedtlsInit == 0)
		return;

	mbedtls_ssl_close_notify(g_ssl_context);
	mbedtls_entropy_free(g_entropy);
	mbedtls_ctr_drbg_free(g_ctr_drbg);
	mbedtls_ssl_config_free(g_ssl_conf);
	mbedtls_ssl_free(g_ssl_context);
	sleep_ms(3000);
	gMbedtlsInit = 0;
}

int reinit_mbedtls(void)
{
	if (gIsTlsConn == 1)
	{
		deinit_mbedtls();
		return init_mbedtls();
	}
	return 0;
}

int mbedtls_handshake(void)
{
    int ret = 1;
	 int rtimeout = gRtSetting->upload.recv_timeout*1000+2000;
	
	 if (gRtSetting->upload.recv_timeout < 10)
		 rtimeout = 10000;

//    TRACE("+++++++++++++Performing the SSL/TLS handshake.. %lld\n", getSysTick());
	 if (gMbedNetFd == COMMON_INTERFACE_UART1)
		ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
	 ret = mbedtls_ssl_session_reset(g_ssl_context);
	 mbedtls_ssl_conf_read_timeout(g_ssl_conf, rtimeout);
	 /*
	 if (ret != 0)
	 {
		 TRACE("mbedtls_ssl_session_reset err:%d\n", ret);
		 while(1);
	 }*/
    while ((ret = mbedtls_ssl_handshake(g_ssl_context)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
//			  TRACE("++++++++++++++++++++++++++++ mbedtls_ssl_handshake returned -0x%x , now:%lld\n\n",
//                (unsigned int)-ret, getSysTick());
            return -1;
        }
    }

//    TRACE("-------------- mbedtls_ssl_handshake ok\n");
	return 0;
}

void mbedtls_set_readTimeout(int tm)
{
	mbedtls_ssl_conf_read_timeout(g_ssl_conf, tm);
}

#define HTTP_SEND(sfd, buf, slen)  \
	do \
	{ \
		isdisconn = 0; \
		if (gIsTlsConn == 1) \
		{ \
			if (mbedtls_ssl_write(g_ssl_context, (uint8 *)buf, slen) != slen) \
				isdisconn = 1; \
		} \
		else \
		{ \
			if (write_n(sfd, buf, slen) != slen) \
				isdisconn = 1; \
		} \
		if (isdisconn == 1 && gRtSetting->upload.hw_inf == Upload_Inf_Ethernet) \
		{ \
			reinit_mbedtls(); \
			gHttpsHandShakeFin = 0; \
			gSocketConnected = 0; \
			goto Reconn; \
		} \
	} \
	while(0)
		

int gHttpsHandShakeFin = 0;
extern int gSocketConnected;
void fire_EEcmdGpoSet()
{
	int i;
	gEEcmdGpoSet.isFire = 0;
	for (i = 0; i < gEEcmdGpoSet.idcnt; ++i)
	{
		gpo_set(gEEcmdGpoSet.ids[i], gEEcmdGpoSet.states[i]);
//		TRACE("gpin:%d, set to %d\n", gEEcmdGpoSet.ids[i], gEEcmdGpoSet.states[i]);
		if (gEEcmdGpoSet.durs[i] == 0)
			gEEcmdGpoSet.finflags[i] = 1;
		else
		{
			gEEcmdGpoSet.finflags[i] = 0;
			gEEcmdGpoSet.isFire = 1;
		}
	}
}
#if Custom_By_ZHXX_ZSYH
void get_EEcmdGpoSet()
{
	int i;
	gEEcmdGpoSet.idcnt = 0;
	if (gRtSetting->gpo_act.count > 0)
	{	
		for (i = 0; i < gRtSetting->gpo_act.count; ++i)
		{
			gEEcmdGpoSet.ids[gEEcmdGpoSet.idcnt] = gRtSetting->gpo_act.ids[i];
			gEEcmdGpoSet.states[gEEcmdGpoSet.idcnt] = gRtSetting->gpo_act.states[i];
			gEEcmdGpoSet.durs[gEEcmdGpoSet.idcnt] = gRtSetting->gpo_act.durs[i];
			gEEcmdGpoSet.idcnt++;
		}
	}	
}
#endif

#if Custom_By_Caipan
void get_EEcmdGpoSet(int status)
{
	int tgid1;
	int tgid2;
	int i;
	
	if (status == 0)
	{
		tgid1 = 1;
		tgid2 = 2;
	}
	else
	{
		tgid1 = 3;
		tgid2 = 4;		
	}
	gEEcmdGpoSet.idcnt = 0;
	
	if (gRtSetting->gpo_act.count > 0)
	{	
		for (i = 0; i < gRtSetting->gpo_act.count; ++i)
		{
			if (gRtSetting->gpo_act.ids[i] == tgid1 || 
				gRtSetting->gpo_act.ids[i] == tgid2)
			{
				gEEcmdGpoSet.ids[gEEcmdGpoSet.idcnt] = gRtSetting->gpo_act.ids[i];
				gEEcmdGpoSet.states[gEEcmdGpoSet.idcnt] = gRtSetting->gpo_act.states[i];
				gEEcmdGpoSet.durs[gEEcmdGpoSet.idcnt] = gRtSetting->gpo_act.durs[i];
				gEEcmdGpoSet.idcnt++;
			}
		}
	}
}
#endif
//int gHttpToken;
void json_remote_cmd(char *cmdbuf, int blen)
{
	json_value *jvalue;
	json_value* pobj;
	int i;
	int validret;
	
#if Custom_By_Caipan
	if (strstr(cmdbuf, "cmd") == NULL)
#elif Custom_By_ZHXX_ZSYH
	if (strstr(cmdbuf, "request") == NULL)
#else
	if (strstr(cmdbuf, "command_type") == NULL)
#endif
		return;

	jvalue = json_parse((char *)cmdbuf, blen);
	if (jvalue != NULL)
	{
		char command_type[50];
#if Custom_By_Caipan
		if (json_getstring(jvalue, "cmd", command_type) == 0)
		{
			char cmdstrbuf[30];
			if (json_getobject(jvalue, "data", &pobj) == 0)
			{
				if (strcmp(command_type, "20002") == 0)
				{
					if (json_getstring(pobj, "time", cmdstrbuf) == 0)
					{
						uint32 servertime;
						if (date_to_seconds(cmdstrbuf, &servertime) == 0)
						{
							gSysSecBase = (uint32)(getSysTick()/1000);
							gUtcSecBase = servertime;
						}
					}
				}
				else if (strcmp(command_type, "20001") == 0)
				{
					int status = -1;
					if (json_getint(jvalue, "data.status", &status) == 0)
					{
						get_EEcmdGpoSet(status);
						fire_EEcmdGpoSet();
					}
				}
			}
		}
#elif Custom_By_ZHXX_ZSYH
		int boolval = 0;
		if (json_getbool(jvalue, "success", &boolval) == 0)
		{
//			TRACE("success:%d\n", boolval);
			if (boolval != 0)
			{
				if (json_getbool(jvalue, "data", &boolval) == 0)
				{
//					TRACE("data:%d\n", boolval);
					if (boolval == 0)
					{
						get_EEcmdGpoSet();
						fire_EEcmdGpoSet();
					}
				}				
			}
		}
#else
		if (json_getstring(jvalue, "command_type", command_type) == 0)
		{			
			if (strcmp(command_type, "set_gpo") == 0)
			{
				//[{"gpo":1, "state":1, "duration":1},{"gpo":2, "state":1, "duration":1}]				
				if (json_getobject(jvalue, "command_data", &pobj) == 0)
				{
					if (gEEcmdGpoSet.isFire == 1)
					{
						TRACE("if (gEEcmdGpoSet.isFire == 1) ---------\n");
						return;
					}
					if (pobj->type == json_array)
					{
						char tmpbuf[40];
						int tmpint;
						
						gEEcmdGpoSet.idcnt = pobj->u.array.length;									
						if (gEEcmdGpoSet.idcnt >= 1 && gEEcmdGpoSet.idcnt <= 5)
						{
							for (i = 0; i < gEEcmdGpoSet.idcnt; ++i)
							{
								sprintf(tmpbuf, "command_data[%d].state", i);
								if (json_getint(jvalue, tmpbuf, &tmpint) != 0)
									return;
								
								if (tmpint < 0 || tmpint > 1)
									return;
								gEEcmdGpoSet.states[i] = tmpint;
								
								sprintf(tmpbuf, "command_data[%d].gpo", i);
								if (json_getint(jvalue, tmpbuf, &tmpint) != 0)
									return;
								if (tmpint < 1 || tmpint > 5)
									return;
								gEEcmdGpoSet.ids[i] = tmpint;
								
								sprintf(tmpbuf, "command_data[%d].duration", i);
								if (json_getint(jvalue, tmpbuf, &tmpint) != 0)
									return;
								if (tmpint < 0 || tmpint > 3600)
									return;
								gEEcmdGpoSet.durs[i] = tmpint*1000;
							}
							
							if (nonrep_uint8_array(gEEcmdGpoSet.ids, gEEcmdGpoSet.idcnt) != 1)
								return;
							
							fire_EEcmdGpoSet();
							/*
							gEEcmdGpoSet.isFire = 0;
							for (i = 0; i < gEEcmdGpoSet.idcnt; ++i)
							{
//								printf("id:%d,state:%d,dur:%d\n", gEEcmdGpoSet.ids[i], 
//									gEEcmdGpoSet.states[i], gEEcmdGpoSet.durs[i]);
								gpo_set(gEEcmdGpoSet.ids[i], gEEcmdGpoSet.states[i]);
								if (gEEcmdGpoSet.durs[i] == 0)
									gEEcmdGpoSet.finflags[i] = 1;
								else
								{
									gEEcmdGpoSet.finflags[i] = 0;
									gEEcmdGpoSet.isFire = 1;
								}
							}
							*/
						}
					}
				}
			}
			/*
			else if (strcmp(command_type, "token") == 0)
			{
				gHttpToken = -1;
				json_getint(jvalue, "command_data", &gHttpToken);
			}*/
			else if (strcmp(command_type, "reboot") == 0)
			{
				system_reset();
			}
			else if (strcmp(command_type, "sync_time") == 0)
			{
				long long servertime;
				if (json_getint64(jvalue, "command_data", &servertime) == 0)
				{					
					gSysSecBase = (uint32)(getSysTick()/1000);
					gUtcSecBase = (uint32)(servertime/1000);
					TRACE("gSysSecBase:%d, gUtcSecBase:%d\n", gSysSecBase, gUtcSecBase);
				}
			}
			else if (strcmp(command_type, "set_static_conf") == 0)
			{
				int reboot;
				
				if (json_getobject(jvalue, "command_data", &pobj) == 0)
				{
					validret = cmd_config_static_settings(pobj, "command_data", cmdbuf, blen, &reboot);
					if (validret == 0 && reboot == 1)
						system_reset();
				}			
			}
			else if (strcmp(command_type, "set_runtime_conf") == 0)
			{
				if (json_getobject(jvalue, "command_data", &pobj) == 0)
				{
					validret = cmd_config_runtime_settings(pobj, "command_data", cmdbuf, blen);
					if (validret == 0)
						system_reset();
				}
			}
			else if (strcmp(command_type, "update_fw_by_ftp") == 0)
			{
				if (json_getobject(jvalue, "command_data", &pobj) == 0)
				{
					//{"mode":1,"server":"192.168.1.44:24","user":"hexp","pwd":"123456","path":"std_hc32only_app_30.0.0.0_8.slfw"}
					BtParams_ST *btparams = malloc_hexp(sizeof(BtParams_ST));
					btparams->updateflag = 'V';
					if (valid_upfw_ftp_params(pobj, btparams) == 0)
					{
						setBtParams(btparams);
						sleep_ms(400);
						system_reset();
					}
					free_hexp(btparams);
				}
			}
		}
#endif
	}
}
//extern int gHttpSendToken;
uint8 gLastHttpFailed = 0;
uint16 gContiHttpFaidedCnt = 0;
void aft_http_recv_failed(int fd)
{
	ioctl(fd, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
	reinit_mbedtls();
	gHttpsHandShakeFin = 0;
	gSocketConnected = 0;
	gLastHttpFailed = 1;
	gContiHttpFaidedCnt++;
}

void http_upload(uint8 *SBuffer, int dlen)
{
	int isdisconn = 0;
	char headerbuf[150];
	int totrecv = 0;
	int rtimeout = gRtSetting->upload.recv_timeout*1000;
	int fd;

	if (gRtSetting->upload.hw_inf == Upload_Inf_Ethernet)
		fd = COMMON_INTERFACE_SOCKET0;
	else
		fd = COMMON_INTERFACE_UART1;
		
Reconn:
//	TRACE("http_upload start ...........\n");
	if (gContiHttpFaidedCnt >= MaxUpsendFailedCntBefReset)
		reset_uart1_ex_dev(&gContiHttpFaidedCnt);
	
	if (gRtSetting->upload.hw_inf == Upload_Inf_Ethernet 
		&& gSocketConnected == 0)
	{
		disconnect(fd);
		close(fd);
		CheckServerConnection();
	}
	
	if (gIsTlsConn == 1 && gHttpsHandShakeFin == 0)
	{
		if (mbedtls_handshake() != 0)
		{
			gSocketConnected = 0;
			sleep_ms(2000);
			gContiHttpFaidedCnt++;
			goto Reconn;
		}
		else
			gHttpsHandShakeFin = 1;			
	}

	if (gIsTlsConn == 0)
	{
		ioctl(fd, COMMON_INTERFACE_SET_TIMEOUT, &rtimeout);
		ioctl(fd, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
	}
	else
		mbedtls_set_readTimeout(rtimeout);
	
	memset(gHttpRespJsonBuf, 0, HttpRespJsonBufLen);
	
	sprintf(headerbuf, "POST %s HTTP/1.1\r\n", 
		gRtSetting->upload.sw_potl_params.http.url+gAbsPathpos);
	HTTP_SEND(fd, headerbuf, strlen(headerbuf));
	sprintf(headerbuf, "Host: %s\r\n", gHostName);
	HTTP_SEND(fd, headerbuf, strlen(headerbuf));
	sprintf(headerbuf, "Accept: */*\r\n");
	HTTP_SEND(fd, headerbuf, strlen(headerbuf));
	sprintf(headerbuf, "Content-Type: application/json; charset=utf-8\r\n");
	HTTP_SEND(fd, headerbuf, strlen(headerbuf));
	sprintf(headerbuf, "Content-Length:%d\r\n\r\n", dlen);
	HTTP_SEND(fd, headerbuf, strlen(headerbuf));
	HTTP_SEND(fd, SBuffer, dlen);
	
	gHttpParseErr = 0;
	gIsFinHttpParse = 0;
	gHttpRespBodyLen = 0;
	http_parser_init(gPHttpParser, HTTP_RESPONSE);

	while (1)
	{
		int nrecv;
		int nparsed;
		if (gIsTlsConn == 0)
			nrecv = read(fd, SockRecvBuffer, CmdRecvBufLen);		
		else
		{
			nrecv = mbedtls_ssl_read(g_ssl_context, SockRecvBuffer, CmdRecvBufLen);
			if (nrecv == MBEDTLS_ERR_SSL_WANT_READ || nrecv == MBEDTLS_ERR_SSL_WANT_WRITE) {
				TRACE("nrecv == MBEDTLS_ERR_SSL_WANT_READ\n");
				continue;
			}
			else if (nrecv == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
				TRACE("nrecv == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY\n");
				aft_http_recv_failed(fd);
				goto Reconn;
			}
		}
		
		if (nrecv <= 0)
		{
//			TRACE("if (nrecv <= 0:%d\n", nrecv);
			aft_http_recv_failed(fd);
			goto Reconn;
		}
//		printf("++++++++++++++++++++++++++ nrecv:%d\n", nrecv);		
//		printf("recvstr:%s\n", m_httpmsgbuffer);

//		memcpy(httprespdumpbuf+totrecv, SockRecvBuffer, nrecv);
		totrecv += nrecv;
		SockRecvBuffer[nrecv] = 0;
		nparsed = http_parser_execute(gPHttpParser, gPHttpParseSet, 
			(char *)SockRecvBuffer, nrecv);
		if (nparsed != nrecv)
		{
			TRACE("nparsed != nrecv err:%d\n", gPHttpParser->http_errno);
			sleep_ms(2000);
//			httprespdumpbuf[totrecv] = 0;
//			TRACE("httprespdumpbuf:%s\n", httprespdumpbuf);
			aft_http_recv_failed(fd);
			goto Reconn;
		}
		if (gPHttpParser->http_errno != HPE_OK)
		{
			TRACE("htParser.http_errno != HPE_OK\n");
			sleep_ms(2000);
//			httprespdumpbuf[totrecv] = 0;
//			TRACE("httprespdumpbuf:%s\n", httprespdumpbuf);
			aft_http_recv_failed(fd);
			goto Reconn;			
		}

		if (gIsFinHttpParse == 1)
		{
//			TRACE("if (gIsFinHttpParse == 1)\n");
			if (gHttpParseErr == 0)
			{
				if (gHttpRespBodyLen > 0)
				{
//					TRACE("totrecv:%d, resp body:%s\n", totrecv, gHttpRespJsonBuf);
					json_remote_cmd(gHttpRespJsonBuf, gHttpRespBodyLen);
//					if (gHttpToken != gHttpSendToken)
//						TRACE("gHttpToken != gHttpSendToken, %d,%d, %lld\n", gHttpSendToken, 
//							gHttpToken, getSysTick());
					
//					gHttpSendToken = rand();
				}
				
				if (gLastHttpFailed == 1 && gIsTlsConn == 0 && 
					gRtSetting->upload.hw_inf != Upload_Inf_Ethernet)
				{
					sleep_ms(gRtSetting->upload.clr_r_buf_time*1000);
					ioctl(fd, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
				}
				gLastHttpFailed = 0;
			}
//			httprespdumpbuf[totrecv] = 0;
//			printf("httprespdumpbuf:%s\n", httprespdumpbuf);
			gContiHttpFaidedCnt = 0;
			break;
		}
	}
}



