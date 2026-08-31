
#include "hc32f46_driver.h"
#include "ModuleReader.h"
#define MSG_CRC_INIT       (0xFFFF)
#define MSG_CCITT_CRC_POLY (0x1021)
#define EastagPage_Addr     (0x7C000)
#define QUELENTH             (32)
#define EPCIDMAXLEN          (16)

#define RED_LED_ON()      (gpo_set(1,1))
#define RED_LED_OFF()     (gpo_set(1,0))
#define GREEN_LED_ON()    (gpo_set(2,1))
#define GREEN_LED_OFF()   (gpo_set(2,0))
#define BLUE_LED_ON()     (gpo_set(3,1))
#define BLUE_LED_OFF()    (gpo_set(3,0))

#define OPTION_INIT        (1)
#define OPTION_ADD         (2)
#define OPTION_DEL         (3)

#define MATCHHEAD (1)
#define MATCHTAIL (0)

 typedef struct
{
	uint8_t ant;
	uint8_t epclen;
	uint8_t PC[2];
	uint8_t epcid[EPCIDMAXLEN];
	unsigned int TimeStamp;
} MsgQueObj_alarm_tag;

 typedef struct
{
	uint8_t frameHead;
	uint8_t ant;
	uint8_t epclen;
	uint8_t PC[2];
	uint8_t EmbededData[EPCIDMAXLEN];
	uint8_t epcid[EPCIDMAXLEN];
	unsigned int TimeStamp;
	unsigned int EmbededDatalen;
} MsgQueObj_HPM6340;

typedef struct  
	{
     uint8_t  frameHead; 
     uint8_t  maclen;
     uint16_t datalen;
     uint8_t  cmdcode;
		 uint8_t  cmdflag;
		 uint8_t  macaddr[6];
		 uint8_t  epclen;
     uint8_t  epc[EPCIDMAXLEN];
     uint16_t crc;		
    }udp_package;
	
typedef struct  
	{
     uint8_t chnum; 
     uint8_t chstate;
     uint8_t start_addr;
     uint8_t match_len;
     char maskcode[32];        
    }rulelist; 
    



typedef struct  
	{
         uint32_t    frameHead;
         uint32_t    totalalarmcnt;
         uint32_t    totaltagcnt;
         uint32_t    easflag;
         uint32_t    radar_range;
		     uint32_t    alarm_volume;
         uint32_t    peoplecount;	
         uint32_t    alarm_duration;
		     uint32_t    alarm_switch;
         uint32_t    tag_read_cnt;		
         char        deviceID[32];
         char        system_time[32];
         rulelist    tagfilter_rule[10];
         uint32_t    random_forest[4];
		     uint32_t    accumulated_time;
		     uint32_t    accumulated_count;
		     uint32_t    opening_time;
		     uint32_t    closing_time;
		     char        tagstoragedays[4];
		     char        remark[4];
         uint32_t    pkgs_cnt;
         uint32_t    crc;
    } rfidcfg;       



 

void tagfiltbuff_init(void);
void get_eastag_to_flash(void); 
void set_eas_defualt(void);    
void rd_idkey_fun(void);
int put_tag_que(TAGINFO *tag);
void HPM6340msg_init(void);
void UDP_upload(MsgQueObj_HPM6340 *udpepc,uint8_t optioncode);
void RollBack(MsgQueObj_HPM6340 *RollBackTag);	
void LED_Runing_Status(uint8_t startbit);
void UDP_SendAll(osMessageQueueId_t sendmsg,uint8_t optioncode);		
void Killepctag(MsgQueObj_HPM6340 *RollBackTag);

