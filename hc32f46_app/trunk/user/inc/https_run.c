#include "hc32f46_driver.h"
#include "mbedtls/platform.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"
#include "http_parser.h"
#include "app_conf.h"
#include <string.h>
//https://autumnfish.cn/song/url?id=1
//#define SERVER_PORT "443"
//#define SERVER_NAME "64.185.227.155"
//checkssl/index?key=APIKEY&domain=www.baidu.com
//#define GET_REQUEST "GET /comment/hot?type=0 HTTP/1.1\r\nHost: autumnfish.cn:443\r\n\r\n"
#define GET_REQUEST "GET /v7/weather/now?location=101010100&key=4444 HTTP/1.1\r\nHost: devapi.qweather.com:443\r\n\r\n"
//#define GET_REQUEST "GET /cc/json/mobile_tel_segment.htm?tel=15850781443 HTTP/1.1\r\nHost: tcc.taobao.com:443\r\n\r\n"
//#define GET_REQUEST "GET /?format=json HTTP/1.1\r\nHost: api4.ipify.org:443\r\n\r\n"
//#define GET_REQUEST "GET /mv/url?id=1 HTTP/1.1\r\nHost: autumnfish.cn:443\r\n\r\n"
//#define GET_REQUEST "GET / HTTP/1.1\r\nHost: www.baidu.com:443\r\n\r\n"
//#define GET_REQUEST "GET /checkssl/index?key=APIKEY&domain=www.baidu.com HTTP/1.1\r\nHost: apis.tianapi.com:443\r\n\r\n"
#define DEBUG_LEVEL 4

static void my_debug(void* ctx, int level,
    const char* file, int line,
    const char* str)
{
    ((void)level);

    printf("%s:%04d: %s", file, line, str);
//    fflush((FILE*)ctx);
}

typedef struct 
{
	void *addr;
	uint16 size;
	uint8 use;
} MemItem_ST;

MemItem_ST gMItems[100];
void init_mem_sta(void)
{
	int i;
	for (i = 0; i < 100; ++i)
		gMItems[i].use = 0;
}

void *calloc_hexp(unsigned int num, unsigned int size)
{
	int i;
	int pos = -1;
	int totmem = 0;
	void *p = calloc(num, size);
	for (i = 0; i < 100; ++i)
	{
		if (gMItems[i].use == 0)
		{
			if (pos == -1)
				pos = i;
		}
		else
			totmem += gMItems[i].size;
	}
	gMItems[pos].addr = p;
	gMItems[pos].size = num*size;
	gMItems[pos].use = 1;
	printf("calloc_hexp tot:%d\n", totmem);
	return p;
}
void free_hexp(void *p)
{
	int i;
	int totmem = 0;
	for (i = 0; i < 100; ++i)
	{
		if (gMItems[i].use == 1)
		{
			if (gMItems[i].addr == p)
				gMItems[i].use = 0;
		}
		
		if (gMItems[i].use == 1)
			totmem += gMItems[i].size;
	}
	printf("free_hexp tot:%d\n", totmem);
	free(p);
}

/**
* @brief   ·µ»ØÊ±¼ä´Á
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

unsigned char httpbuf[1024];
int usrc216_net_send( void *ctx, const unsigned char *buf, size_t len )
{
	int ret;
//	printf("usrc216_net_send len:%d\n", len);
	ret = write(COMMON_INTERFACE_UART1, buf, len);
//	printf("after usrc216_net_send\n");
	return ret;
	 
}
int usrc216_net_recv_timeout( void *ctx, unsigned char *buf,
                              size_t len, uint32_t timeout )
{
	int ret;
	int timeout_ = timeout;
	ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_SET_TIMEOUT, &timeout_);
	ret = read(COMMON_INTERFACE_UART1, buf, len);
	if (ret == -2)
		ret = MBEDTLS_ERR_SSL_TIMEOUT;

	return ret;
}

mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
mbedtls_ssl_context ssl;
mbedtls_ssl_config conf;


int init_mbedtls(void)
{
	int ret = 1;
	const char* pers = "ssl_client1";
	
#if defined(MBEDTLS_DEBUG_C)
    mbedtls_debug_set_threshold(DEBUG_LEVEL);
#endif
	
	mbedtls_ssl_init(&ssl);
	mbedtls_ssl_config_init(&conf);
	mbedtls_ctr_drbg_init(&ctr_drbg);
	mbedtls_printf("\n  . Seeding the random number generator...");
	mbedtls_entropy_init(&entropy);
   if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
		(const unsigned char*)pers,
		strlen(pers))) != 0) {
		mbedtls_printf(" failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret);
		return -1;
	}
	mbedtls_printf(" ok\n");
	mbedtls_printf("  . Setting up the SSL/TLS structure...");

	if ((ret = mbedtls_ssl_config_defaults(&conf,
		MBEDTLS_SSL_IS_CLIENT,
		MBEDTLS_SSL_TRANSPORT_STREAM,
		MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		mbedtls_printf(" failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n", ret);
      return -1;
    }

    mbedtls_printf(" ok\n");

	 mbedtls_ssl_conf_read_timeout(&conf, 5000);
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_conf_dbg(&conf, my_debug, stdout);

    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
        mbedtls_printf(" failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret);
        return -1;
    }
	 
//	 mbedtls_ssl_set_hostname(&ssl, SERVER_NAME);
	 mbedtls_ssl_set_bio(&ssl, NULL, usrc216_net_send, NULL, usrc216_net_recv_timeout);
	 return 0;
}

extern int gHttpRespBodyLen;
extern int gHttpParseErr;
extern int gIsFinHttpParse;
extern http_parser *gPHttpParser;
extern char httprespdumpbuf[1024*2];
extern http_parser_settings *gPHttpParseSet;
extern char *gHttpRespJsonBuf;

int https_test(void)
{
	int totrecv = 0;
	int err = 0;
	memset(gHttpRespJsonBuf, 0, 256);
	strcpy((char *)httpbuf, GET_REQUEST);
	mbedtls_ssl_write(&ssl, httpbuf, strlen(GET_REQUEST));

	gHttpParseErr = 0;
	gIsFinHttpParse = 0;
	gHttpRespBodyLen = 0;
	http_parser_init(gPHttpParser, HTTP_RESPONSE);
	
	while (1)
	{
		int nrecv;
		int nparsed;
		nrecv = mbedtls_ssl_read(&ssl, SockRecvBuffer, CmdRecvBufLen);	
		if (nrecv == MBEDTLS_ERR_SSL_WANT_READ || nrecv == MBEDTLS_ERR_SSL_WANT_WRITE) {
			printf("nrecv == MBEDTLS_ERR_SSL_WANT_READ\n");
			continue;
		}
		else if (nrecv == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
			printf("nrecv == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY\n");
			err = -1;
			goto FIN;
		}
//		printf("++++++++++++++++++++++++++ nrecv:%d\n", nrecv);
		
//		printf("recvstr:%s\n", m_httpmsgbuffer);
		if (nrecv <= 0)
		{
			printf("if (nrecv <= 0) :%d\n", nrecv);
			err = -1;
			goto FIN;
		}
		memcpy(httprespdumpbuf+totrecv, SockRecvBuffer, nrecv);
		totrecv += nrecv;
		SockRecvBuffer[nrecv] = 0;
		nparsed = http_parser_execute(gPHttpParser, gPHttpParseSet, 
			(char *)SockRecvBuffer, nrecv);
		if (nparsed != nrecv)
		{
			printf("nparsed != nrecv err:%d\n", gPHttpParser->http_errno);
			sleep_ms(2000);
			httprespdumpbuf[totrecv] = 0;
			printf("httprespdumpbuf:%s\n", httprespdumpbuf);
			ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
			err = -1;
			goto FIN;
		}
		if (gPHttpParser->http_errno != HPE_OK)
		{
			printf("htParser.http_errno != HPE_OK\n");
			sleep_ms(2000);
			httprespdumpbuf[totrecv] = 0;
			printf("httprespdumpbuf:%s\n", httprespdumpbuf);
			ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
			err = -1;
			goto FIN;
		}

		if (gIsFinHttpParse == 1)
		{
//			printf("if (gIsFinHttpParse == 1)\n");
			if (gHttpParseErr == 0 && gHttpRespBodyLen >0)
				printf("totrecv:%d, resp body:%s\n", totrecv, gHttpRespJsonBuf);
			httprespdumpbuf[totrecv] = 0;
//			printf("httprespdumpbuf:%s\n", httprespdumpbuf);
			break;
		}
	}

FIN:
	if (err != 0)
	{
//		mbedtls_ssl_session_reset(&ssl);
		
		mbedtls_ssl_close_notify( &ssl );
		mbedtls_ssl_free(&ssl);
		mbedtls_ssl_config_free(&conf);
		mbedtls_ctr_drbg_free(&ctr_drbg);
		mbedtls_entropy_free(&entropy);
		
	}
	return err;
}

void deinit_mbedtls(void)
{
		mbedtls_ssl_close_notify( &ssl );
		mbedtls_ssl_free(&ssl);
		mbedtls_ssl_config_free(&conf);
		mbedtls_ctr_drbg_free(&ctr_drbg);
		mbedtls_entropy_free(&entropy);	
}

int https_handshake(void)
{
    int ret = 1;
    mbedtls_printf("  . Performing the SSL/TLS handshake...");

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            mbedtls_printf(" failed\n  ! mbedtls_ssl_handshake returned -0x%x\n\n",
                (unsigned int)-ret);
            return -1;
        }
    }

    mbedtls_printf(" ok\n");
	return 0;
}

int https_run(void)
{
    int ret = 1, len;

    mbedtls_printf("  . Performing the SSL/TLS handshake...");

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            mbedtls_printf(" failed\n  ! mbedtls_ssl_handshake returned -0x%x\n\n",
                (unsigned int)-ret);
            return -1;
        }
    }

    mbedtls_printf(" ok\n");


    /*
     * 3. Write the GET request
     */
    mbedtls_printf("  > Write to server:");
//    fflush(stdout);

    len = sprintf((char*)httpbuf, GET_REQUEST);

    while ((ret = mbedtls_ssl_write(&ssl, httpbuf, len)) <= 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            mbedtls_printf(" failed\n  ! mbedtls_ssl_write returned %d\n\n", ret);
            return -1;
        }
    }

    len = ret;
    mbedtls_printf(" %d bytes written\n\n%s", len, (char*)httpbuf);

    /*
     * 7. Read the HTTP response
     */
    mbedtls_printf("  < Read from server:");
//    fflush(stdout);

    do {
        len = sizeof(httpbuf) - 1;
        memset(httpbuf, 0, sizeof(httpbuf));
        ret = mbedtls_ssl_read(&ssl, httpbuf, len);

        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
        }

        if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            break;
        }

        if (ret < 0) {
            mbedtls_printf("failed\n  ! mbedtls_ssl_read returned %d\n\n", ret);
            break;
        }

        if (ret == 0) {
            mbedtls_printf("\n\nEOF\n\n");
            break;
        }

        len = ret;
        mbedtls_printf(" %d bytes read\n\n%s", len, (char*)httpbuf);
    } while (1);

//    mbedtls_ssl_close_notify(&ssl);
	return 0;
}

