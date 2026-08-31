#ifndef _TAG_BUFFER_ARM7_H
#define  _TAG_BUFFER_ARM7_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "ModuleReader.h"

void initTbBuffer(int datalen);
int tagInsert(TAGINFO *tag);
int tagGetCnt(void);
int tagGetNext(TAGINFO *tag);
void setRecHighestRssi(int is_);
void setUniByAnt(int is_);
void setUniByEmdData(int is_);
unsigned short crc_Msg(unsigned char *msg, int len);

#ifdef __cplusplus
}
#endif

#endif





