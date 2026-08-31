#ifndef __FLASH_H__
#define __FLASH_H__

#include "type.h"
#include "hc32_common.h"
/* C binding of definitions if building with C++ compiler */
#ifdef __cplusplus
extern "C"
{
#endif


#define EFM_LATENCY_0                   (0ul)
#define EFM_LATENCY_1                   (1ul)
#define EFM_LATENCY_2                   (2ul)
#define EFM_LATENCY_3                   (3ul)
#define EFM_LATENCY_4                   (4ul)
#define EFM_LATENCY_5                   (5ul)
#define EFM_LATENCY_6                   (6ul)
#define EFM_LATENCY_7                   (7ul)
#define EFM_LATENCY_8                   (8ul)
#define EFM_LATENCY_9                   (9ul)
#define EFM_LATENCY_10                  (10ul)
#define EFM_LATENCY_11                  (11ul)
#define EFM_LATENCY_12                  (12ul)
#define EFM_LATENCY_13                  (13ul)
#define EFM_LATENCY_14                  (14ul)
#define EFM_LATENCY_15                  (15ul)
	
typedef enum en_efm_status
{
    EfmOk           = 0x00u,
    EfmError        = 0x01u,
    EfmBusy         = 0x02u,
    EfmTimeout      = 0x03u,
} en_efm_status_t;


void EFM_SetWaitCycle(uint32_t u32Cycle);
void EFM_SetLatency(uint32_t u32Latency);
en_efm_status_t EFM_Unlock(void);
en_efm_status_t EFM_Lock(void);
en_efm_status_t EFM_WaitForOperationDone(uint32_t u32Timeout);
	
int flash_sector_erase(uint32 dest);
void flash_bytes_read(uint32 addr,void *buf,uint16 len);
int flash_bytes_write(uint32 dest,void *buf, uint16 len);


#ifdef __cplusplus
}
#endif

#endif /* __FLASH_H__ */

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
