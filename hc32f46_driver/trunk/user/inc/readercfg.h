#ifndef _READER_CFG_H_
#define _READER_CFG_H_

#define SilionMACBase1 0x08
#define SilionMACBase2 0x26
#define SilionMACBase3 0xAE
#define SilionMACBase4 0x10


extern const uint8 DefIpAddr[];
extern const uint8 DefSubnetMask[];
extern const uint8 DefGateWay[];
extern const uint8 DefDnsServer[];
extern const uint8 DefMac[6];
//const uint8 DefMac[6] = {0x08, 0x26, 0xAE, 0x10, 0x45, 0x5E};
extern const uint16 DefTcpPort;
//#define NetConfig_Len 24


#define MaxNetConfigLen 100
#define MaxWifiConfigLen 256
#define MaxHwTypeConfigLen 32
#define MaxMonetConfigLen 224
#define MaxBluetoothConfigLen 128
#define MaxActiveModeConfigLen 2048
#define MaxPassiveModeConfigLen 2048
//#define MaxModbusPasivConfigLen 760
#define MaxBootConfigLen 320
#define MaxWorkModeParamLen 64

#define NetConfig_Marker "NetConfig"
#define RdrStaticSets_Marker "StaticSettings"
#define WorkModeParams_Marker "WorkModeParams"
#define BtParams_Marker "BtParams_v2.0"
#define ActModeCfg_Marker "StdModeActNewV2"
#define WifiConfig_Marker "WifiConfig"
#define HwTypeConfig_Marker "HwTypeConfig"
#define MonetConfig_Marker "MonetConfig"
#define BluetoothConfig_Marker "BluetoothConfig"

typedef struct
{
	uint8 *netcfgbuf;
	uint8 *wificfgbuf;
	uint8 *hwtypecfgbuf;
	uint8 *monetcfgbuf;
	uint8 *bluetoothcfgbuf;
	uint8 *activemodecfgbuf;
	uint8 *workmodeparambuf;
	uint8 *passivemodecfgbuf;
	uint8 *bootcfgbuf;
} ConfReadBuffer;

#endif

