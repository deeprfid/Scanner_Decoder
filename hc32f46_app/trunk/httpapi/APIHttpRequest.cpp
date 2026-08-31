#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hc32f46_driver.h"
#include "APIHttpRequest.h"
#include "Utility.h"


APIHttpRequest::APIHttpRequest()
{
	http_parser_settings_init(&m_hpsettings);
	m_hpsettings.on_header_value = OnHeaderValueCallback;
	m_hpsettings.on_url = OnUrlCallback;
	m_hpsettings.on_message_complete = OnMessageCompleteCallback;
	m_hpsettings.on_body = OnBodyCallback;
	m_hpsettings.on_header_field = OnHeaderFieldCallback;
	m_hpsettings.on_headers_complete = OnHeadersCompleteCallback;
}

int APIHttpRequest::Parse(char *threebytes)
{
	int pos = 0;
	m_iscontentlen = false;
	m_isfinpars = false;
	m_lasthttpheaderstate = HttpParseHeader_None;
	m_contentlen = 0;
	m_url[0] = 0;
	bool IsFirstRecv = true;
	http_parser_init(&m_htParser, HTTP_REQUEST);
	m_htParser.data = this;
	if (threebytes != NULL && IsFirstRecv)
	{
		memcpy(m_httpmsgbuffer, threebytes, 3);
		pos += 3;
	}
	while (true)
	{
		int nrecv;
		if (IsFirstRecv)
		{
			nrecv = read_n(m_sock, m_httpmsgbuffer+pos, HTTP_URLEN);			
		}
		else
			nrecv = read(m_sock, m_httpmsgbuffer, MAXHTTPMSGBUFLEN);
					
		if (nrecv <= 0)
		{
//			TRACE("0000000 nrecv:%d\n", nrecv);
			return -1;
		}
		
		if (IsFirstRecv)
		{
			if (threebytes != NULL)
				nrecv += 3;
			IsFirstRecv = false;
		}

		m_httpmsgbuffer[nrecv] = 0;
//		TRACE("nrecv:%d, recvstr:%s\n", nrecv, m_httpmsgbuffer);
		
		int nparsed = http_parser_execute(&m_htParser, &m_hpsettings, 
			m_httpmsgbuffer, nrecv);
		if (nparsed != nrecv)
		{
//			TRACE("11111111 nparsed != nrecv err:%s\n", http_errno_description(HTTP_PARSER_ERRNO(&m_htParser)));
			return -1;
		}
		if (m_htParser.http_errno != HPE_OK)
		{
//			TRACE("22222222 htParser.http_errno != HPE_OK\n");
			return 400;
		}

		if (m_isfinpars)
			break;
	}
	return 200;
}

int APIHttpRequest::OnUrlCallback(http_parser *parser, 
								  const char *at, size_t length)
{
	APIHttpRequest *pReq = (APIHttpRequest*)parser->data;
	if (length > HTTP_URLEN - 1)
	{
		pReq->m_respcode = 400;
		return -1;
	}

	memcpy(pReq->m_url, at, length);
	pReq->m_url[length] = 0;
	strcpy(pReq->m_method, http_method_str((http_method)parser->method));
//	printf("method:%s\n", pReq->m_method);
//	printf("url:%s\n", pReq->m_url);
	return 0;
}

int APIHttpRequest::OnHeaderFieldCallback(http_parser *parser, 
										  const char *at, size_t length)
{
	APIHttpRequest *pReq = (APIHttpRequest*)parser->data;
	if (pReq->m_lasthttpheaderstate == HttpParseHeader_None || 
		pReq->m_lasthttpheaderstate == HttpParseHeader_Value)
	{
		if (pReq->m_lasthttpheaderstate == HttpParseHeader_Value)
		{
			pReq->m_headervalue[pReq->m_headervaluepos] = 0;
//			printf("val:%s\n", pReq->m_headervalue);
		}
		if (length <= HTTP_HEADERFIELDLEN)
		{
			memcpy(pReq->m_headerfield, at, length);
			pReq->m_headerfieldpos = length;
		}
		else
			pReq->m_headerfieldpos = 0;
	}
	else
	{
		if (length + pReq->m_headerfieldpos <= HTTP_HEADERFIELDLEN)
		{
			memcpy(pReq->m_headerfield+pReq->m_headerfieldpos, at, length);
			pReq->m_headerfieldpos += length;
		}
	}
	pReq->m_lasthttpheaderstate = HttpParseHeader_Field;
	return 0;
}

int APIHttpRequest::OnHeaderValueCallback(http_parser *parser, const char *at, size_t length)
{	
	APIHttpRequest *pReq = (APIHttpRequest*)parser->data;

	if (pReq->m_lasthttpheaderstate == HttpParseHeader_Field)
	{
		pReq->m_headerfield[pReq->m_headerfieldpos] = 0;
		if (strcmp("Content-Length", pReq->m_headerfield) == 0)
			pReq->m_iscontentlen = true;
		if (length <= HTTP_HEADERVALUELEN)
		{
			memcpy(pReq->m_headervalue, at, length);
			pReq->m_headervaluepos = length;
		}
		else
			pReq->m_headervaluepos = 0;
	}
	else
	{
		if (length + pReq->m_headervaluepos <= HTTP_HEADERVALUELEN)
		{
			memcpy(pReq->m_headervalue+pReq->m_headervaluepos, at, length);
			pReq->m_headervaluepos += length;
		}
	}
	pReq->m_lasthttpheaderstate = HttpParseHeader_Value;
	return 0;
}

int APIHttpRequest::OnBodyCallback(http_parser *parser, 
								   const char *at, size_t length)
{
	APIHttpRequest *pReq = (APIHttpRequest*)parser->data;
	
		if (pReq->m_contentlen+length > HTTP_POSTJSONLEN -1)
		{
			pReq->m_respcode = 400;
			return -1;
		}
		memcpy(pReq->m_postjson+pReq->m_contentlen, at, length);
		pReq->m_contentlen += length;
	
	return 0;
}
int APIHttpRequest::OnMessageCompleteCallback(http_parser *parser)
{
	APIHttpRequest *pReq = (APIHttpRequest*)parser->data;
	pReq->m_postjson[pReq->m_contentlen] = 0;
//	printf("m_contentlen:%d\n", pReq->m_contentlen);
//	printf("MessageComplete\n");
	pReq->m_respcode = 200;
	pReq->m_isfinpars = true;
	return 0;
}
int APIHttpRequest::OnHeadersCompleteCallback(http_parser *parser)
{
	APIHttpRequest *pReq = (APIHttpRequest*)parser->data;
	pReq->m_contentlen = 0;

	pReq->m_headervalue[pReq->m_headervaluepos] = 0;
//	printf("val:%s\n", pReq->m_headervalue);

	return 0;
}

APIHttpRequest::HttpMethodType APIHttpRequest::Method()
{
	if (strcmp("POST", m_method) == 0)
		return HttpMethod_POST;
	else if (strcmp("OPTIONS", m_method) == 0)
		return HttpMethod_OPTIONS;
	return HttpMethod_None;
}
int APIHttpRequest::RespCode()
{
	return m_respcode;
}
