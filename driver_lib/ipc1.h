
#include "hc32f46_driver.h"

#define MSG_CRC_INIT (0xFFFF)
#define MSG_CCITT_CRC_POLY (0x1021)
#define EastagPage_Addr     (0x7C000)
#define FRAMEHead           (0x1201)
#define TAGITEMLENGTH       (33)
#define MAX_SPI_BUFF_LEN    (512U)
#define SPI_SOC_TRANSFER_COUNT_MAX  (512U)
#define SPI_JSONRAW_MAX      (500U)
#define JSONPDUHEAD          (0xF8F7)
#define SYSTEMCFGHEAD        (0xF8E7D6C5)
#define SYSTEMGETHEAD        (0xF1F2F3F4)


    
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
         uint8_t frameHead;
         uint8_t readernamelen;
         uint8_t datafieldlen[2];
         uint8_t commandtype;
         uint8_t commandflag;
         uint8_t statuscode[4];
         uint8_t randomdata[486];
         uint32_t uuid;
         uint32_t gpducnt;
         uint32_t crc;
         uint32_t random_forest;
    } hpm_pdu;


typedef struct  
	{
         uint16_t frameHead;
         uint16_t index;
         uint16_t bytecnt;
         uint32_t tagtotalcnt;
         uint8_t  tagitem[500];
         uint16_t crc;
    } jsonrawdata; 

typedef struct  
	{
         uint32_t    frameHead;
         uint32_t    tagstoragedays;
         uint32_t    totalalarmcnt;
         uint32_t    totaltagcnt;
         uint32_t    easflag;
         uint32_t    radarcfg;
         char        deviceID[32];
         char        system_time[32];
         rulelist    tagfilter_rule[10];
         uint32_t    random_forest[14];
         uint32_t    pkgs_cnt;
         uint32_t    crc;
    } rfidcfg;     

typedef struct  
	{
         uint8_t pdulen[2];
         uint8_t frameHead[4];
         uint8_t datafieldlen;
         uint8_t appcommand;
         uint8_t registered[64];
         uint8_t crc[2];
         uint8_t random_forest[4];
    } lkt_pdu;  

 

void tagfiltbuff_init(void);
void get_eastag_to_flash(void); 
void set_eas_defualt(void);    
void rd_idkey_fun(void);
void ipc_hpm_message(uint8_t *SpiBuffer, uint8_t dlen);
void data_noising(uint32_t *inbuf, uint32_t *outbuf, uint32_t noise, uint16_t len);
void data_denoising(uint32_t *inbuf, uint32_t *outbuf, uint32_t noise, uint8_t len);
uint8_t hpm_op(uint8_t *wtbuf,uint8_t *rdbuf,uint16_t buflen);
uint8_t hpm_op_read(uint8_t *rdbuf,uint16_t buflen);
    



