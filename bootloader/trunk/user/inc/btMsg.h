#ifndef Bt_Msg_hexp_
#define Bt_Msg_hexp_

 /*
 host to mcu: 一个字节（oxff，固定头），数据段长度（两个字节）,命令码（一个字节）, 数据（变长），CRC（两个字节）
 mcu to host: 一个字节（0xff, 固定头），数据段长度（两个字节）,命令码（一个字节），命令执行状态码（两个字节），数据（变长），CRC（两个字节）
 */

typedef struct
{
	unsigned char MsgCode;
	unsigned short Datalen;
	unsigned char data[300];
	unsigned char StatusCode[2];
	unsigned char CRC[2];
} BtMsgSt, *PBtMsgSt;

typedef enum 
{
	BOOTLOADER_VERSION = 0x00,
	Soft_Version = 0x01,
	Write_FlashPage = 0x02,
	Read_FlashPage = 0x03,
	Verify_Firmware = 0x04,
	Verify_BtParams = 0x05,
	Set_Relay_Com = 0x06, //数据段的头两个字节表示中转指令的超时时间，单位为毫秒
	Relay_Cmd = 0x07,
	
	Boot_Firmware = 0x08,
	Module_PowerOn = 0x09,
	Module_PowerOff = 0x0a,

	SPI_FLASH_ERASE		= 0x0b,
	SPI_FLASH_WRITE_PAGE	= 0x0c,
	SPI_FLASH_READ_PAGE	= 0x0d,
	SPI_FLASH_VERIFY_DATA	= 0x0e,
	

	HC32F46X_SEND_DATA_INPAGE = 0x11,
	HC32F46X_WRITE_PAGE = 0x12,
	HC32F46X_WRITE_BTPARAMS = 0x13,
	HC32F46X_VERIFY_FIRMWARE = 0x14,
	
} BtMsgCode;

typedef int (*RecvFunc)(unsigned char *, int, int);
typedef int (*SendFunc)(unsigned char *, int, int);

typedef int (*RecvFunc2)(unsigned char *, int);

int SendRespMsg(PBtMsgSt Msg);

extern int gSocks[2];
extern int gTcpport;
extern int gStatusFlags[2];

int RecvMsg(PBtMsgSt Msg);

unsigned short rfid_calcCrc(unsigned char *pMsg);
#endif
