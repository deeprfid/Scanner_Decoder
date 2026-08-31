#ifndef _UPDATE_BY_FTP_
#define _UPDATE_BY_FTP_

int ftp_data_callback(uint8 *buf, int len);
int ftp_init_callback(void);
int usr_ftp_update_fw(char *seraddr, uint16 port, 
	char *user, char *pwd, char *path);

extern uint32 gFlhfilelen;
extern uint32 gFlhfilever;
extern uint32 gFlhwaddr;
extern uint8 gCrcsAndModFwDes[];
void aft_ftp_update_fw(BtParams_ST *btparams);
#endif


