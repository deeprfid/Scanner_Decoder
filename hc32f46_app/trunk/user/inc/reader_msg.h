#ifndef _READER_MSG_H_
#define _READER_MSG_H_
#include <time.h>
#include "hc32f46_driver.h"
#include "ModuleReader.h"
#include "app_conf.h"

void RemoteCmd(void);
//void send_error(void);
//void send_pulse(int justpoweron);
int AddTag2SockBuffer(unsigned char *SBuffer, TAGINFO *tag, int pos);
int AddTag2SockBuffer_j(char *Jbuf, TAGINFO *tag);
void AddTagCnt2SockBuffer(unsigned char *SBuffer, int tagcnt, int pos);
void SetMsgDatalen(unsigned char *SBuffer, int totallen);
int AddMsgHeader2SockBuffer(unsigned char *SBuffer, MidMsgType mtype, int ecode);
int AddMsgHeader2SockBuffer_j(char *Jbuf, MidMsgType mtype);
void up_send(unsigned char *SBuffer, int dlen);
int write_n(int fd, void *buf, int len);
void CheckServerConnection(void);
int GpiChange(uint8 *gsts);
void send_evt_gpichan(uint8 state);
void send_evt_heartbeat(void);
void send_evt_tagcoming(TAGINFO *tag);
void send_evt_reader_err(void);
void send_evt_emptydata(void);
void send_evt_synctimereq(void);
void send_evt_tagbatch(void);

typedef struct
{
	unsigned char main_board;
	unsigned char rfid_mod;
	unsigned char software_version[4];
	unsigned char antcount;
	unsigned char connected_antennas[16];
	unsigned int hb_count;
} HeartBeatData_ST;

extern HeartBeatData_ST gHbData;
extern unsigned char gGpiMap;
extern volatile uint32 gUtcSecBase;
extern volatile uint32 gSysSecBase;

void seconds_to_date(time_t secs, char *date);
int date_to_seconds(char *date, uint32 *secs);
void reset_uart1_ex_dev(uint16 *failcnt);

#if Custom_By_Caipan
extern char gMsg_IPStr[20];
extern char gMsg_MACStr[20];
#endif

#endif


