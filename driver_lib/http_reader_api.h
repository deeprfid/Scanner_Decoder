#ifndef _HTTP_READER_API_H
#define _HTTP_READER_API_H

#ifdef __cplusplus
extern "C" {
#endif

void httpapi_init(void);
void httpapi_hander(int fd, char *threebytes);
int httpapi_openrdr(int *hreader);
int httpapi_create_buffer(int itcnt, int msize);

#ifdef __cplusplus
}
#endif

#endif


