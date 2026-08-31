#ifndef _READER_INIT_H_
#define _READER_INIT_H_

#include "ModuleReader.h"
READER_ERR OpenReader(void);
int HandleModErr(void);
int init_upload(void);
extern int uart0_bauds[];
extern int uart0_bindex;
extern unsigned char gIsUnknownMod;

#endif


