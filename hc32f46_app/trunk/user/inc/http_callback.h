#ifndef _HTTP_CALL_BACK_H
#define _HTTP_CALL_BACK_H
#include "hc32f46_driver.h"
#include "mbedtls/platform.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

void init_http_fn(void);
void http_upload(uint8 *SBuffer, int dlen);
int url_get_domain(char *url, char *domain, 
	unsigned short *port, int *abrpos, int *ishttps);

void preinit_mbedtls(void);
int init_mbedtls(void);
int mbedtls_handshake(void);
void deinit_mbedtls(void);
int reinit_mbedtls(void);
void mbedtls_set_readTimeout(int tm);
void init_mem_sta(void);

extern mbedtls_entropy_context *g_entropy ;
extern mbedtls_ctr_drbg_context *g_ctr_drbg;
extern mbedtls_ssl_context *g_ssl_context;
extern  mbedtls_ssl_config *g_ssl_conf;

int http_upload_test(int fd, uint8 *SBuffer, int dlen);
void json_remote_cmd(char *cmdbuf, int blen);
extern int gAbsPathpos;
extern int gMbedNetFd;
extern int gIsTlsConn;
#endif


