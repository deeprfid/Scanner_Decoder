#ifndef APIHttpResponse_H
#define APIHttpResponse_H
#include "HttpModuleAPI.h"
#include "APIHttpRequest.h"

class APIHttpResponse
{
public:
	static void httpRespose(HttpModuleAPI *pHMApi, APIHttpRequest *ApiReq, int curfd, char *threebytes);
	static int SendJson(int sock, char *pjson);
	static int SendErrorHeader(int sock, int httpcode);
	static int SendOptionsReqHeader(int sock);
	static int SendFile(int sock, char *filename);

	static int SendChunkedHeader(int sock);
	static int SendChunkedSection(int sock, char *chunk, int len);
	static int SendChunkedTrailer(int sock);
	static bool m_IsReboot;
private:
	static int sendSameHeaders(int sock, bool iscache=true);
};


#endif

