
#include <stdio.h>
#include <stdlib.h>
#include "Utility.h"
#include "APIHttpRequest.h"
#include "APIHttpResponse.h"
#include "HttpModuleAPI.h"
#include "hc32f46_driver.h"
#include "mp_pool.h"
#include "http_reader_api.h"
#include "ModuleReader.h"

HttpModuleAPI *pHttpMAPI = NULL;
APIHttpRequest *pHttpReq = NULL;

void httpapi_init(void)
{
	hrlErrInfo = (char *)malloc_hexp(100);
	pHttpMAPI = new HttpModuleAPI();
	add_new_mem_sta(sizeof(HttpModuleAPI));
	TRACE("pHttpMAPI:%p\n", pHttpMAPI);
	pHttpReq = new APIHttpRequest();
	add_new_mem_sta(sizeof(APIHttpRequest));
	TRACE("pHttpReq:%p\n", pHttpReq);
}
int httpapi_create_buffer(int itcnt, int msize)
{
	return pHttpMAPI->create_buffer(itcnt, msize);
}

int httpapi_openrdr(int *hreader)
{
	HttpMApiErrCode err;
	for (int i = 0; i < 3; ++i)
	{
		err = pHttpMAPI->Init();
		if (err == HMApiErr_Ok)
		{
			*hreader = pHttpMAPI->getReaderHandle();
			return 0;
		}
		else if (err == (HttpMApiErrCode)(httpAPIErrCodeBase+MT_TEST_DEV_FAULT_5))
			return -1;
	}
	return -1;
}

void httpapi_hander(int fd, char *threebytes)
{
	APIHttpResponse::httpRespose(pHttpMAPI, pHttpReq, fd, threebytes);
}
/*
int user_main_httpapi(void)
{
//	HttpMApiErrCode initHttpErr = HMApiErr_Ok;

	apt_pair_socks_st apt_st;
	int m_rtm = 50;
	int curconnfd;
	
	printf("before hrlErrInfo = (char *)malloc(100)\n");
	hrlErrInfo = (char *)malloc(100);
	printf("before mp_init()\n");
	mp_init();
	memset(&apt_st, 0, sizeof(apt_st));
	apt_st.sns[0] = COMMON_INTERFACE_SOCKET0;
	apt_st.sns[1] = COMMON_INTERFACE_SOCKET1;
	apt_st.port = 8080;
	printf("before ioctl\n");
	ioctl(COMMON_INTERFACE_SOCKET0, COMMON_INTERFACE_SET_TIMEOUT, &m_rtm);
	ioctl(COMMON_INTERFACE_SOCKET1, COMMON_INTERFACE_SET_TIMEOUT, &m_rtm);
	
	pHttpMAPI = new HttpModuleAPI();
	pHttpReq = new APIHttpRequest();
	
	if (pHttpMAPI->Init() != HMApiErr_Ok)
	{
		printf("pHttpMAPI->Init() != HMApiErr_Ok\n");
		while(1);
	}
	printf("before apt_pair_select_ex\n");
	while (true)
	{
		curconnfd =  apt_pair_select_ex(&apt_st);
		APIHttpResponse::httpRespose(pHttpMAPI, pHttpReq, curconnfd, NULL);
	}
	
	return 0;
}
*/

