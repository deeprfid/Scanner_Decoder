

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "hc32_ddl.h"
#include "hc32f46_driver.h"









uint32_t Ucode_read(uint8_t *inbuf,uint16_t u32len)
{
uint32_t crcdata=0;

stc_efm_unique_id_t efm_unique_id={0};

efm_unique_id=EFM_ReadUID();

uint32_t idkey=0;
uint32_t u32temp=0x12011201;//God Father's Day

idkey=efm_unique_id.uniqueID1^efm_unique_id.uniqueID2^efm_unique_id.uniqueID3;

crcdata=idkey^u32temp;

return crcdata;
} 
/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
