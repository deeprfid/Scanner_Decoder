#ifndef APIHttpRequest_H
#define APIHttpRequest_H
#include "http_parser.h"


class APIHttpRequest
{
public:
	typedef enum
	{
		HttpMethod_None    = 0,
		HttpMethod_GET     = 1,
		HttpMethod_POST    = 2,
		HttpMethod_OPTIONS = 3,
	} HttpMethodType;

	APIHttpRequest();

	int Parse(char *threebytes);
	HttpMethodType Method();
	int RespCode();
	void setSocket(int sock)
	{
		m_sock = sock;
	}
	int getSocket()
	{
		return m_sock;
	}
	char *Url()
	{
		return m_url;
	}
	char *Body()
	{
		return m_postjson;
	}
	int BodyLen()
	{
		return m_contentlen;
	}

private:
	typedef enum
	{
		HttpParseHeader_None = 0,
		HttpParseHeader_Field = 1,
		HttpParseHeader_Value = 2,
	} HttpParseHeaderState;


#define HTTP_URLEN 50
#define HTTP_POSTJSONLEN 1024*20
#define MAXHTTPMSGBUFLEN 1024*5
#define HTTP_HEADERFIELDLEN 30
#define HTTP_HEADERVALUELEN 80
#define HTTP_METHODLEN 20

	http_parser_settings m_hpsettings;
	static int OnUrlCallback(http_parser *parser, const char *at, size_t length);
	static int OnHeaderFieldCallback(http_parser *parser, const char *at, size_t length);
	static int OnHeaderValueCallback(http_parser *parser, const char *at, size_t length);
	static int OnBodyCallback(http_parser *parser, const char *at, size_t length);
	static int OnMessageCompleteCallback(http_parser *parser);
	static int OnHeadersCompleteCallback(http_parser *parser);

	char m_url[HTTP_URLEN];
	int m_contentlen;
	char m_postjson[HTTP_POSTJSONLEN];
	char m_httpmsgbuffer[MAXHTTPMSGBUFLEN];
	bool m_isfinpars;
	HttpParseHeaderState m_lasthttpheaderstate;
	int m_headerfieldpos;
	int m_headervaluepos;

	char m_headerfield[HTTP_HEADERFIELDLEN];
	char m_headervalue[HTTP_HEADERVALUELEN];
	char m_method[HTTP_METHODLEN];
	int m_respcode;
	int m_sock;
	http_parser m_htParser;
	bool m_iscontentlen;
};


#endif

