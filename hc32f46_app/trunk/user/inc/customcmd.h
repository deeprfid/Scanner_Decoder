#ifndef _CUSTOM_CMD_H
#define  _CUSTOM_CMD_H

#ifdef __cplusplus
extern "C" {
#endif

void custom_getip(unsigned char *buf);
void custom_setip(unsigned char *buf);
void custom_gpiget(unsigned char *buf);
void custom_gposet(unsigned char *buf);
void custom_gpiget2(unsigned char *buf);
void custom_gposet2(unsigned char *buf);
void custom_resetmodule(int uart1fd, unsigned char *buf);
void custom_setm6ebaud230400(int uart1fd, unsigned char *buf);
void custom_getconfig(unsigned char *buf);
void custom_setconfig(unsigned char *buf);
int custom_setactparams(unsigned char *buf);
	
int SaveCurStaticSettings(void);

#ifdef __cplusplus
}
#endif

#endif
