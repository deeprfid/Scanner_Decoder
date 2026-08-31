#ifndef _HC32F46_DRIVER_H
#define _HC32F46_DRIVER_H

#include "type.h"
#include "socket.h"
#include "rtx_os.h"
#include "json-parser.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
	
#define SYSTEM_TICK_DUR  5


#define O_BLOCK  0
#define O_NONBLOCK  1

#define TOTAL_UART_NUM 		4
#define TOTAL_SOCKET_NUM 		4

#define COMMON_INTERFACE_UART0	100 // uart0 
#define COMMON_INTERFACE_UART1	101 // uart1
#define COMMON_INTERFACE_UART2	102 // uart1
#define COMMON_INTERFACE_UART3	103 // uart1

#define COMMON_INTERFACE_USB0	110 // usb 0 hid
#define COMMON_INTERFACE_USB1	111 // usb 1 cdc
	
#define COMMON_INTERFACE_SOCKET0				0 	 // socket 0
#define COMMON_INTERFACE_SOCKET1				1 	 // socket 1
#define COMMON_INTERFACE_SOCKET2				2 	 // socket 2
#define COMMON_INTERFACE_SOCKET3				3 	 // socket 3

#define COMMON_INTERFACE_SET_ALLPARA	1 // uart 设置所有参数，参数对应commonUartPara 结构
#define COMMON_INTERFACE_SET_ISBLOCK	2 // uart 设置是否阻塞，类型uint32
#define COMMON_INTERFACE_SET_ISPRINTF	3 // uart0 设置是否支持打印，类型uint32
#define COMMON_INTERFACE_SET_TIMEOUT	4 // uart 设置超时时间，参数对应0x0 - 0xffff，类型uint32
#define COMMON_INTERFACE_SET_BAUDRATE	5 // uart 设置波特率，类型uint32
#define COMMON_INTERFACE_CLEAR_REVBUF	10 // uart 清空接收缓冲区，类型uint32

#define COMMON_INTERFACE_PEEK_DATA_SIZE 11

typedef struct
{
	uint8	isBlock;	// 0  O_BLOCK 阻塞;      1  O_NONBLOCK 非阻塞;
	uint8	isPrintf;	// 0  禁止打印; 1 支持打印 ；仅 uart0 支持打印
	uint8	mode;		// see <AT91SAM7X256.H> 
	uint8	databits;	// see <AT91SAM7X256.H> 
	uint8	stopbits;	// see <AT91SAM7X256.H> 
	uint8 parity;
	uint8 flowctrl;
	int	timeout;	//0x0 - 0xffff ; 无数据阻塞时间  = timeout*10ms
	uint8 isRdam;
	uint8 t485;
	uint32	baudrate;	//波特率
} commonUartPara;

typedef struct
{
	uint8	isBlock;
	int	timeout; //小于等于0, 表示永久
} commonSocketPara;

typedef struct
{
	uint8	isBlock;
	int	timeout; //小于等于0, 表示永久
} commonUsbPara;

typedef struct
{
	uint8	ip[4];//192.168.1.100
	uint8	subnetMask[4];//255.255.255.0
	uint8	gatewayIP[4];//192.168.1.1
	uint8	mac[6];	
	uint16	listenPort;//默认8080
	uint8	dnsServer[4];
} networkParaConfig;

extern int gIsConfDhcp;
extern networkParaConfig gNetConf;
unsigned long long getSysTick(void);

int get_network_config(networkParaConfig *para);
int set_network_config(networkParaConfig *para);
int is_netconf_dhcp(networkParaConfig *para);
void set_default_network_config(void);
void dump_network_config(networkParaConfig *para);
void erase_active_mode_params(void);
void get_passivemode_config(int offset, uint8 *pData, int len);
int set_passivemode_config(int offset, uint8 *pData, int len);

unsigned short GetNumU16(uint8 *p);
unsigned int GetNumU32(uint8 *p);
void SetNumU16(uint8 *p, uint16 num);
void SetNumU32(uint8 *p, uint32 num);
int IsIpv4address(const char *addr);
int atoi_Arm(const char *nptr);

int  read(int s, void *buf, uint32 len);
/*
	read_n 函数
	s: 需读入数据的句柄
	buf : 数据缓存区
	len: 读入数据的长度
	返回值：>0 表示读到的数据长度；-1 表示读数据超时
*/
int read_n(int s, void *buf, uint32 len);
/*
	write 函数
	s: 需写入数据的句柄
	buf : 数据缓存区
	len: 写入数据的长度
	返回值：>=0 表示写入数据的长度；-1 表示写入出错
*/
int  write(int s, const void *buf, uint32 len);

int uart_open(int uart, void *paraAddr);
int uart_close(int s);

/*
	ioctl 函数
	s: 设备句柄
	cmd: 操作命令，例如COMMON_INTERFACE_UART_SET_ALLPARA
	paraAddr: dev 对应的参数地址
	返回值：0 表示操作成功；-1 表示操作失败
*/
int ioctl(int s, uint32 cmd, void *paraAddr);

/*flash operation*/
int flash_sector_erase(uint32 dest);// 8K 擦除;  ,必须是8*1024的整数倍
void flash_bytes_read(uint32 dest,void *buf,uint16 len); //读FLASH ;  
int flash_bytes_write(uint32 dest,void *buf, uint16 len); //写FLASH  ;
void system_reset(void);

int network_init(int dhcpsn);
void set_default_ip(void);

typedef void (*apt_pair_CreateConn_Callback)(void);
typedef void (*apt_pair_CloseConn_Callback)(void);
typedef struct
{
	int statusflags[2];
	apt_pair_CreateConn_Callback create_conn_cb;
	apt_pair_CloseConn_Callback close_conn_cb;
	int sns[2];
	int port;
} apt_pair_socks_st;

int apt_pair_select_nob_ex(apt_pair_socks_st *apt_st);
int apt_pair_select_ex(apt_pair_socks_st *apt_st);

int apt_pair_select_nob(int *sns, int port, int *statusflags);
int apt_pair_select(int *sns, int port, int *statusflags);
int apt_single_select_nob(int sn, int port, uint64 *lastacttime, int acttimeout);
int apt_single_select(int sn, int port, uint64 *lastacttime, int acttimeout);

void os_dly_wait(int tenticks); //为了兼容性，不建议再使用


#ifdef _DEBUG
#define TRACE printf
#else
#define TRACE(out, ...)   
#endif


void sleep_ms(int ms);

void firmware_version(uint8 *version);
//void firmware_upgrade(uint8 flag);


//void *align8byte(void *addr, int size, int *newsize);
void wait_fin_init(void);

void pre_DNS_init(void);
void aft_DNS_run(void);
void DNS_init(uint8_t s, uint8_t * buf);
int8_t DNS_run(uint8_t * dns_ip, uint8_t * name, uint8_t * ip_from_dns);

typedef int (*ftpInitCallback)();
typedef int (*ftpDataCallback)(uint8_t *, int);
void ftpc_init(uint8_t * ser_ip, char *user, 
	char *password, char *filepath, uint16 port, 
		ftpInitCallback initcb, ftpDataCallback datacb);
	
int ftpc_run(uint8_t * dbuf);
void ftpc_destory(void);

int dhcp_wait(void);

void RCC_Configuration(void);
void  GPIO_Configuration(void);
void timer_Init(void);


typedef enum
{
	FwUpdateMode_Default = 0,
	FwUpdateMode_ByFtp_FmEth = 1,
	FwUpdateMode_ByFtp_Fm4G = 2,
	FwUpdateMode_ByHttp_FmEth = 3,
	FwUpdateMode_ByHttp_FmUartEx = 4,
} FwUpdateModeCode;

#define BTFWUPD_FTP_USER_BUFLEN 50
#define BTFWUPD_FTP_PASSWORD_BUFLEN 50
#define BTFWUPD_FTP_SERADDR_BUFLEN 50
#define BTFWUPD_FTP_FILENAME_BUFLEN 100

typedef struct
{
	uint8 updateflag;
	uint32 firmwareaddr;
	uint32 firmwaresize;
	uint32 firmwarecrc;
	uint32 firmwarever;
	FwUpdateModeCode updatemode;
	char ftpuser[BTFWUPD_FTP_USER_BUFLEN];
	char ftppassword[BTFWUPD_FTP_PASSWORD_BUFLEN];
	char ftpaddr[BTFWUPD_FTP_SERADDR_BUFLEN];
	char filename[BTFWUPD_FTP_FILENAME_BUFLEN];
	uint32 btParamscrc;
} BtParams_ST;

void dumpBtParams(BtParams_ST *params);
int setBtParams(BtParams_ST *params);
int getBtParams(BtParams_ST *params);
void firmware_upgrade(uint8 flag);
typedef void (*BtnResetCallback)(void);
extern BtnResetCallback gBtnResetCb;

void rfid_power_on(void);
void rfid_power_off(void);
void beep_on(void);
void beep_off(void);
void led_on(void);
void led_off(void);

void gpo_set(uint8 gpoid, uint8 state);
uint8 gpi_get(uint8 gpoid);
uint8 gpi_get_all(void);
int get_ipreset_key_value(void);

typedef struct 
{
	unsigned char bank;
	unsigned short start_bit;
	unsigned short mask_len;
	unsigned char mask[64];
	unsigned char is_match;
} Rdr_Tagfilter;

typedef struct
{
	unsigned char bank;
	unsigned char start_block;
	unsigned char blockcnt;
	unsigned char pwd[4];
} Rdr_InvBankRead;

typedef struct
{
	unsigned char gpi_count;
	unsigned char gpi_ids[4];
	unsigned char gpi_states[4];
} Rdr_GpiTrigger;

typedef struct
{
	unsigned char name_len;
	unsigned char name[129];
	unsigned char rpwrs_len;
	unsigned short rpwrs[16];
	unsigned char region;
	unsigned char hoptab_len;
	int frepoints[50];
	unsigned char gen2session;
	unsigned char gen2q;
	unsigned char is_tagdata_uni_byant;
	unsigned char is_tagdata_uni_bybank;
	unsigned char is_record_max_rssi;
	
	unsigned char invants_len;
	unsigned char inv_ants[16];
	unsigned short heart_beat_cylce;
	unsigned char is_tagfilter;
	Rdr_Tagfilter tag_filter;
	
	unsigned char is_inv_bank_read;
	Rdr_InvBankRead inv_bank_read;
	
	unsigned char upload_ip[4];
	unsigned short upload_port;
	unsigned char conn_mode;
	unsigned char ack_client_mode;

	unsigned char data_aggr_mode;
	int data_aggr_duration;

	unsigned char is_gpi_trigger;
	unsigned char gpi_trigger_mode;
	int gpi_read_timeout;
	Rdr_GpiTrigger gpi_trigger1;
	Rdr_GpiTrigger gpi_trigger2;
	
	unsigned char event_count;
	unsigned char events[10];
	
	unsigned short inv_cycle;
	unsigned short interval_cycle;
	unsigned char max_rec_databytes_length;
	unsigned char cusparam_len;
	unsigned char cusparam[65];
	
	unsigned char gpo_init_states;
	
	unsigned char loc_gpo_act_count;
	unsigned char lga_gpo_id[4];
	unsigned char lga_gpo_states[4];
	int lga_gpo_durs[4];
	
} AppCustomParams;

int get_active_mode_config(AppCustomParams *pACP);
void dump_active_mode_config(AppCustomParams *pACP);
void setdef_active_mode_config(AppCustomParams *pACP);

#define ERASE_FLS_CFG_BIT_ETHERNET 0x0001
#define ERASE_FLS_CFG_BIT_WLAN 0x0002
#define ERASE_FLS_CFG_BIT_HWTYPE 0x0004
#define ERASE_FLS_CFG_BIT_BLUETOOTH 0x0008
#define ERASE_FLS_CFG_BIT_ACTMODE 0x0010
#define ERASE_FLS_CFG_BIT_WKMODEPARA 0x0020
#define ERASE_FLS_CFG_BIT_PSVMODE 0x0040

int erase_multi_config(uint16 erasebits);
int set_multi_config(uint8 *netBytes, int netLen, 
	uint8 *pasveBytes, int pasveLen, uint8 *actBytes, int actLen);

typedef struct
{
	networkParaConfig network;
	uint8 modtype[2];
	uint8 bdfwver[4];
	uint8 modfwver[4];
	uint8 workmode;
} BRDCST_DevInfo;
extern BRDCST_DevInfo gBrdCstDevInfo;

int brdcst_conf_init(int sn);
void brdcst_conf_handler(void);

int addr_str2bin(char *ipstr, uint8 *ipbins);
int InitDegutPrintf(int type, int sn, char *hostip, unsigned short port);
void DisableDegutPrintf(void);

int getMaxSocketId(void);
void init_osThreadAttr_t(osThreadAttr_t *attr, int stacksize, osPriority_t prio);
int SetFlashConfig(unsigned char *pData, int datalen, networkParaConfig *netconfig);
int GetFlashConfig(unsigned char *pData, int *datalen);

void led_toggle(int dur, int cycle, void (*cb)(void));

int init_usb(int type);
void send_key(uint8_t key, int isupper);
int IsUsbAvailable(void);
int apt_usb_select(void);
int apt_usb_select_nob(void);
int apt_uart_select_nob(int *uarts, int ucnt);
int apt_multi_infs_select(apt_pair_socks_st *apt_st, int *uarts, 
	int uartcnt, int *socks, int sockcnt);
int apt_multi_infs_select_nob(apt_pair_socks_st *apt_st, int *uarts, 
	int uartcnt, int *socks, int sockcnt);
/*
typedef enum
{
	Wlan_Auth_OPEN = 0,
	Wlan_Auth_WPAPSK = 1,
	Wlan_Auth_WPA2PSK = 2,
	Wlan_Auth_SHARED = 3,
} Wlan_Auth_Code;

typedef enum
{
	Wlan_Encry_NONE = 0,
	Wlan_Encry_TKIP = 1,
	Wlan_Encry_AES = 2,
	Wlan_Encry_WEP_A = 3,
	Wlan_Encry_WEP_H = 4,
} Wlan_Encry_Code;
*/

typedef enum
{
	Wlan_WMode_None = 0,
	Wlan_WMode_AP = 1,
	Wlan_WMode_STA = 2,
	Wlan_WMode_AP_STA = 3,
} Wlan_WMode_Code;

typedef struct
{
	networkParaConfig ipinfo;
	char ssid[33];
	char pwd[65];
	uint8 mode;
} WlanConfig_ST;
void setdef_wlan_config(WlanConfig_ST *wlanst);
int erase_wlan_config(void);
int valid_wlan_config(char *buf, int blen, WlanConfig_ST *wlanst);
int get_wlan_config(WlanConfig_ST *wlanst);
int set_wlan_config(WlanConfig_ST *wlanst);
void dump_wlan_config(WlanConfig_ST *wlanst);
void tojson_wlan_config(WlanConfig_ST *wlanst, char *jstart, int *len);
extern networkParaConfig gWlanNet;

typedef struct
{
	char name[33];
	uint8 std_ble_pair;
	uint8 pwd_pair;
	uint8 mac[6];
	char pwd[7];
} BluetoothConfig_ST;

void setdef_bluetooth_config(BluetoothConfig_ST *blest);
int erase_bluetooth_config(void);
int valid_bluetooth_config(char *buf, int blen, BluetoothConfig_ST *blest);
int get_bluetooth_config(BluetoothConfig_ST *blest);
int set_bluetooth_config(BluetoothConfig_ST *blest);
void dump_bluetooth_config(BluetoothConfig_ST *blest);
void tojson_bluetooth_config(BluetoothConfig_ST *blest, 
	char *jstart, int *len);

#define MONET_APN_NAME_LEN 51
#define MONET_APN_USER_LEN 51
#define MONET_APN_PWD_LEN 51
typedef struct
{
	char name[MONET_APN_NAME_LEN];
	char user[MONET_APN_NAME_LEN];
	char pwd[MONET_APN_NAME_LEN];
	uint8 auth;
	uint8 cid;
	uint8 enable;	
} MonetApnConfig_ST;

typedef struct
{
	MonetApnConfig_ST apn;
} MonetConfig_ST;

void setdef_monet_config(MonetConfig_ST *monst);
int erase_monet_config(void);
int valid_monet_config(char *buf, int blen, MonetConfig_ST *monst);
int get_monet_config(MonetConfig_ST *monst);
int set_monet_config(MonetConfig_ST *monst);
void dump_monet_config(MonetConfig_ST *monst);
void tojson_monet_config(MonetConfig_ST *monst, 
	char *jstart, int *len);

typedef struct
{
	short read_power;
	short write_power;
} tx_power_st;

typedef struct
{
	uint8 address;
	uint8 type;
	uint8 stop_bits;
	uint8 data_bits;
	uint8 parity;
	uint8 flow_ctrl;
	int baud;
} rdr_st_set_uart;

typedef struct
{
	short session;
	short q;
	short profile;
	short target;
} tag_potl_gen2;

typedef struct
{
	tag_potl_gen2 gen2;
} rdr_st_set_protocol;

typedef struct
{
	tx_power_st tx_powers[16];
	short region;
	int ant_max_dwell_time;
	short hop_mode;
	int hop_table_cnt;
	int hop_table[50];
} rdr_st_set_rf;

typedef struct
{
	short unique_by_antenna;
	short unique_by_bank_data;
	short record_highest_rssi;
	short max_tb_rec_len;
} rdr_st_set_tag_data;

typedef networkParaConfig rdr_st_set_ethernet;
typedef WlanConfig_ST rdr_st_set_wlan;
typedef BluetoothConfig_ST rdr_st_set_ble;
typedef MonetConfig_ST rdr_st_set_monet;

typedef union
{
	rdr_st_set_monet monet;
	rdr_st_set_wlan wlan;
	rdr_st_set_ble ble;
} rdr_st_set_uart_ex;

typedef enum
{
	rdr_st_set_op_range_None = 0,
	rdr_st_set_op_range_PassiveOnly = 1,
	rdr_st_set_op_range_ActiveOnly = 2,
	rdr_st_set_op_range_Both = 3,
} rdr_st_set_op_range_code;
typedef enum
{
	rdr_st_set_usb_type_None = 0,
	rdr_st_set_usb_type_KeyHid = 1,
	rdr_st_set_usb_type_HidCdc = 2,
} rdr_st_set_usb_type_code;

typedef struct
{
	uint8 usb_type;
	uint8 max_tb_rec_len;
	uint8 evt_que_len;
} rdr_st_set_app_init;

typedef struct
{
	int ants[16];
	int ants_cnt;
	uint16 cycle;
	uint16 interval;
	int inv_mode;
} tagops_param_inventory;

typedef struct
{
	uint8 bank;
	uint8 start;
	uint8 blkcnt;
	uint8 is_bankdata;
} tagops_param_bankdata;

typedef struct 
{
	uint8 bank;
	uint16 start;
	uint16 mask_len;
	uint8 mask[64];
	uint8 match;
	uint8 is_tagfilter;
} tagops_param_tagfilter;

typedef struct
{
	uint8 ant;
	uint16 timeout;
	uint8 aespwd[4];
} tagops_param_access;

typedef struct
{
	tagops_param_inventory inventory;
	tagops_param_bankdata bankdata;
	tagops_param_tagfilter tagfilter;
	tagops_param_access accessop;
	uint16 mb_sinv_tag_fmt;
} tagops_param_st;

typedef struct
{
	uint8 reset;
	uint8 reboot;
	tagops_param_st tagops_param;
	rdr_st_set_app_init app_init;
	rdr_st_set_ethernet ethernet;
	uint8 is_ethernet;
	rdr_st_set_uart_ex uart_ex;
	uint8 uart_ex_type;
	rdr_st_set_protocol protocol;
	rdr_st_set_rf rf;
	rdr_st_set_tag_data tag_data;
	uint8 gpos[5];
	rdr_st_set_uart uart1;
	rdr_st_set_uart uart2;
} ReaderStaticSettings_ST;

int get_rdr_static_settings(ReaderStaticSettings_ST *static_set);
int valid_static_settings(char *buf, int blen, ReaderStaticSettings_ST *static_set);
void dump_static_settings(ReaderStaticSettings_ST *static_set);
int set_rdr_static_settings(ReaderStaticSettings_ST *static_set);
void tojson_rdr_static_settings(ReaderStaticSettings_ST *static_set, 
	char *jstart, int *len, int isexcfg);
//int valid_modbus_uart(char *buf, int blen, rdr_st_set_modbus_uart *moduart);
extern int gAntNumber;
void setdef_rdr_static_settings(ReaderStaticSettings_ST *static_set);
int cmd_config_static_settings(json_value *jvalue, char *cfg_name, 
	char *buf, int blen, int *reboot);

typedef enum 
{
	MidMsgType_None = 0,
	MidMsgType_TagRead = 1,
	MidMsgType_GpiTrigger = 2,
	MidMsgType_TagComing = 3,
	MidMsgType_HeartBeat = 4,
	MidMsgType_RdrError = 5,
	MidMsgType_SyncTimeReq = 6,
		
	MidMsgType_GetConf = 20,
	MidMsgType_SetConf = 21,
	MidMsgType_GetGPI= 22,
	MidMsgType_SetGPO = 23,
	MidMsgType_Reboot = 24,
	MidMsgType_DetectBoardExt = 25,
	MidMsgType_GetBoardExt = 26,
	MidMsgType_Update_Fw_By_Ftp = 27,
	
	MidMsgType_GetStaticConf = 30,
	MidMsgType_SetStaticConf = 31,
	MidMsgType_EraseReaderConf = 32,
	MidMsgType_SaveCurStaticConf = 33,
	MidMsgType_GetCurWorkMode = 34,
	MidMsgType_SwitchWorkMode = 35,
	MidMsgType_GetRunTimeConf = 36,
	MidMsgType_SetRunTimeConf = 37,
	MidMsgType_GetWlanConf = 38,
	MidMsgType_SetWlanConf = 39,
	MidMsgType_GetBluetoothConf = 40,
	MidMsgType_SetBluetoothConf = 41,
	
	MidMsgType_TestUart1ex = 100,
} MidMsgType;

int strTohex(const char *buf, int len, unsigned char *hexbuf);
void hexTostr(const uint8 *buf, int len, char *strbuf);
void toupper_arm(char *str);
int nonrep_int_array(int *arr, int cnt);
int nonrep_uint8_array(uint8 *arr, int cnt);
void memcpy_byb(void *dst, const void *src, size_t len);

typedef enum
{
	WorkMode_None = 0,
	WorkMode_Passive = 1,
	WorkMode_ActVer_1 = 2,
	WorkMode_ActVer_2 = 3,
	WorkMode_ActVer_3 = 4,
} WorkMode_Code;

int get_workmode_params(WorkMode_Code *wmode);
int set_workmode_params(WorkMode_Code wmode);
WorkMode_Code TestFwType_ex(void);
int TestFwType(void);

#define Mqtt_MaxHostLen 49

typedef struct
{
	char ser_ip[Mqtt_MaxHostLen];
	uint16 ser_port;
} upload_tcp_params;


#define MaxUploadHttpUrlLen 257
typedef struct
{
	char url[MaxUploadHttpUrlLen];
} upload_http_params;


#define Mqtt_MaxUserLen 33
#define Mqtt_MaxPwdLen 33
#define Mqtt_MaxSubBrdCTpLen 49
#define Mqtt_MaxSubUniCTpLen 49
#define Mqtt_MaxPubTpLen 49

typedef struct{
	char host[Mqtt_MaxHostLen];
	char user[Mqtt_MaxUserLen];
	char pwd[Mqtt_MaxPwdLen];
	int kal_time;
	uint16 port;
	char sub_b_topic[Mqtt_MaxSubBrdCTpLen];
	char sub_u_topic[Mqtt_MaxSubUniCTpLen];
//	int potl_ver;
	char pub_topic[Mqtt_MaxPubTpLen];
	uint8 sub_b_qos;
	uint8 sub_u_qos;
	uint8 pub_qos;
	uint8 tls;
} upload_mqtt_params;

typedef enum
{
	Wg_Type_None = 0,
	Wg_Type_26 = 1,
	Wg_Type_34 = 2,
	Wg_Type_66 = 3,
}Wiegand_Type_Code;

typedef struct{
	uint8 pls_width;
	uint8 pls_interval;
	uint8 data_interval;
	uint8 type;
	uint8 bytes_order;
} upload_wiegand_params;

typedef union
{
	upload_tcp_params tcp;
	upload_http_params http;
	upload_mqtt_params mqtt;
	upload_wiegand_params wiegand;
} rdr_rt_set_sw_potl_params;

typedef struct
{
	uint8 mode;
	int timeval;
} rdr_rt_set_data_aggr;
typedef enum
{
	Upload_Inf_None = 0,
	Upload_Inf_Ethernet = 1,
	Upload_Inf_Uart_1 = 2,
	Upload_Inf_HidKb = 3,
	Upload_Inf_4G = 4,
	Upload_Inf_Wifi = 5,
	Upload_Inf_Wiegand = 6,
	Upload_Inf_Uart_2 = 7,
} Upload_Inf_Code;

typedef enum
{
	Upload_Trans_Potl_None = 0,
	Upload_Trans_Potl_Tcp = 1,
	Upload_Trans_Potl_Http = 2,
	Upload_Trans_Potl_Mqtt = 3,
} Upload_Trans_Potl_Code;

typedef struct
{
	rdr_rt_set_data_aggr data_aggr;
	rdr_rt_set_sw_potl_params sw_potl_params;
	uint8 hw_inf;
	uint8 sw_potl;
	uint8 client_ack;
	uint8 recv_timeout;
	uint8 crc_enable;
	uint8 clr_r_buf_time;
} rdr_rt_set_upload;

typedef struct
{
	uint8 count;
	uint8 ids[4];
	uint8 states[4];
} rdr_rt_set_gpi_cond;

typedef struct
{
	uint8 is_gpi_trigger;
	uint8 mode;
	uint8 cond_order;
	int timeval;
	rdr_rt_set_gpi_cond cond_1;
	rdr_rt_set_gpi_cond cond_2;
	int timeval2;
} rdr_rt_set_gpi_trigger;

typedef struct
{
	uint8 len;
	uint8 param[129];
} rdr_rt_set_cus_param;

typedef struct
{
	uint8 count;
	uint8 ids[5];
	uint8 states[5];
	int durs[5];
} rdr_rt_set_gpo_act;

typedef enum
{
	rdr_rt_evt_None = 0,
	rdr_rt_evt_TagRead = 1,
	rdr_rt_evt_HeartBeat = 2,
	rdr_rt_evt_GpiChange = 3,
	rdr_rt_evt_EmptyData = 4,
	rdr_rt_evt_TagComing = 5,
	rdr_rt_evt_SyncTimeReq = 6,
} rdr_rt_evt_code;

typedef struct
{
	uint8 count;
	uint8 ids[10];
} rdr_rt_set_events;

typedef struct
{
	char name[129];
	uint16 hb_cylce;
	uint16 s_buf_size;
} rdr_rt_set_glob_params;;

typedef struct
{
	uint8 reset;
	uint16 tag_json_format;
	rdr_rt_set_glob_params glob_params;
	rdr_rt_set_upload upload;
	rdr_rt_set_gpi_trigger gpi_trigger;
	rdr_rt_set_cus_param cus_param;
	rdr_rt_set_gpo_act gpo_act;
	rdr_rt_set_events events;
} ReaderRunTimeSettings_ST;

void AppCustomParams_To_static_settings(AppCustomParams *pAppCusPara, 
	ReaderStaticSettings_ST *static_set);
void AppCustomParams_To_runtime_settings(AppCustomParams *pAppCusPara, 
	ReaderRunTimeSettings_ST *runtime_set);
void dump_runtime_settings(ReaderRunTimeSettings_ST *rt_set);
int set_rdr_runtime_settings(ReaderRunTimeSettings_ST *rt_set);
int get_rdr_runtime_settings(ReaderRunTimeSettings_ST *rt_set);
void tojson_rdr_runtime_settings(ReaderRunTimeSettings_ST *rt_set, char *jstart, int *len);
void setdef_rdr_runtime_settings(ReaderRunTimeSettings_ST *rt_set);
int valid_runtime_settings(char *buf, int blen, ReaderRunTimeSettings_ST *rt_set);
int check_runtime_settings(ReaderRunTimeSettings_ST *rt_set);
int cmd_config_runtime_settings(json_value *jvalue, char *cfg_name, char *buf, int blen);

int custom_ee_commond(int rfd, unsigned char *buf, int mode, 
	int (* save_cur_set)(void), int (* sendfunc)(int, uint8 *, int), 
	uint16 (* crc_msg)(uint8 *, int), ReaderRunTimeSettings_ST *prtset);


		
int init_4g(int baud, char *domain, uint16 serport, int waitconn, int waittm);
int init_4g_noconf(int baud, int waittime);
int check_4g(int baud);
int init_wlan(int baud, char *domain, uint16 serport, int waitconn, int waittm);
int check_wlan(int baud, char *ssid, char *pwd);
int wlan_reconn_ser(void);
int init_bluetooth(int baud);
int get_wlan_wslk(void);
void ex_power_on(void);
void ex_power_off(void);

typedef enum
{
	Uart_Ex_None = 0,
	Uart_Ex_4G = 1,
	Uart_Ex_Wlan = 2,
	Uart_Ex_Bluetooth = 3,
} Uart_Ex_Code;

typedef enum
{
	Spi_Ex_None = 0,
	Spi_Ex_Ethernet = 1,
} Spi_Ex_Code;

typedef struct
{
	uint8 spi_ex;
	uint8 uart_ex; //0,没有外界扩展，1扩展4g，2扩展wifi，3扩展蓝牙
} BoardComponents_ST;

int get_board_compos(void);
int set_board_compos(Spi_Ex_Code spiex, Uart_Ex_Code uart1ex);
Uart_Ex_Code detect_uart_ex_dev(void);
Spi_Ex_Code detect_spi_ex_dev(void);
Spi_Ex_Code get_spi_ex_dev(void);
Uart_Ex_Code get_uart_ex_dev(void);

void wiegand_init(void);
void wiegand_send26(uint8 PwWith, uint8 PwCyc, uint8 *dat);
void wiegand_send34(uint8 PwWith, uint8 PwCyc, uint8 *dat);
void wiegand_send66(uint8 PwWith, uint8 PwCyc, uint8 *dat);

int g4_reconn_ser(void);

void init_mem_sta(void);
void *malloc_hexp(unsigned int size);
void *calloc_hexp(unsigned int num, unsigned int size);
void free_hexp(void *p);
void add_new_mem_sta(int size);
int get_left_heap_size(char *prefix);


typedef struct
{
	uint8 idcnt;
	uint8 ids[5];
	uint8 states[5];
	uint8 finflags[5];
	int durs[5];
	volatile int isFire;
} EERCmdGpoSet_ST;
extern EERCmdGpoSet_ST gEEcmdGpoSet;

typedef void (*cycle_task) (void *data);
int add_cycle_task(cycle_task task, void *data);
int valid_upfw_ftp_params(json_value *pobj, BtParams_ST *btparams);


void adc_init(void);
int adc_get_val(float *adc);


    


void HashConfig(void);
void TrngInitConfig(void);
void trng_create(uint8_t *trngbuf,uint8_t u8Length);
void active_http_post(void);
void deviceID_update(ReaderRunTimeSettings_ST *prtset);
uint32_t Ucode_read(uint8_t *inbuf,uint16_t u32len);
int UDP_send(uint8_t * buf, uint16_t len);
void rgb_led_toggle(uint8_t gpoid,int dur, int cycle, void (*cb)(void));
void Erase_eastag_to_flash(void);
#ifdef __cplusplus
}
#endif

#endif



