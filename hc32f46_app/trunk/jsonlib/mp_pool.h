#ifndef mp_pool_H
#define mp_pool_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include "ModuleReader.h"

void mp_init(int msize);
//void mp_init_ex(void *p_pool, void *p_blockflag, void *p_mallocmaker, int msize);
void mp_init_ex(void *p_pool, int msize);
void *mp_malloc(size_t, int zero);
void mp_free(void *);
void mp_resetpool(void);

void buildTbBuffer(int datalen, unsigned char *buf, int msize);
void initTbBuffer(int datalen, int msize);
void setIsUseMutex(int is_);
void tagClear(void);
int tagInsert(TAGINFO *tag);
int tagGetCnt(void);
int tagGetNext(TAGINFO *tag);
void setRecHighestRssi(int is_);
void setUniByAnt(int is_);
void setUniByEmdData(int is_);
void setIsUseMutex(int is_);
int tagPeekNext(TAGINFO *tag);
unsigned short crc_Msg(unsigned char *msg, int len);

#ifdef __cplusplus
}
#endif

#endif

