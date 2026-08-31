#ifndef __FLASH_H__
#define __FLASH_H__

#include "hc32_common.h"

/* C binding of definitions if building with C++ compiler */
#ifdef __cplusplus
extern "C"
{
#endif
	
#define FLASH_SECTOR_SIZE      0x2000ul
#define FLASH_SIZE            (64u * FLASH_SECTOR_SIZE)
#define RAM_SIZE               0x2F000ul

#define ApplicationAddress     0x0000A000
#define OEMAdd                 0x0000A000
#define OEMADDR                0x0000A000
	

#define PAGE_SIZE             (0x2000)    /* 8 Kbyte */
#define FLASH_BASE            ((uint32_t)0x00000000) /*!< FLASH base address in the alias region */
#define SRAM_BASE             ((uint32_t)0x1FFF8000) /*!< SRAM base address in the alias region */
	
#define EFM_FLAG_WRPERR                 0x00000001
#define EFM_FLAG_PEPRTERR               0x00000002
#define EFM_FLAG_PGSZERR                0x00000004
#define EFM_FLAG_PGMISMTCH              0x00000008
#define EFM_FLAG_EOP                    0x00000010
#define EFM_FLAG_RWERR                  0x00000020
#define EFM_FLAG_RDY                    0x00000100

	
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


#define IS_VALID_FLASH_LATENCY(x)                                              \
(   ((x) == EFM_LATENCY_0)                      ||                             \
    ((x) == EFM_LATENCY_1)                      ||                             \
    ((x) == EFM_LATENCY_2)                      ||                             \
    ((x) == EFM_LATENCY_3)                      ||                             \
    ((x) == EFM_LATENCY_4)                      ||                             \
    ((x) == EFM_LATENCY_5)                      ||                             \
    ((x) == EFM_LATENCY_6)                      ||                             \
    ((x) == EFM_LATENCY_7)                      ||                             \
    ((x) == EFM_LATENCY_8)                      ||                             \
    ((x) == EFM_LATENCY_9)                      ||                             \
    ((x) == EFM_LATENCY_10)                     ||                             \
    ((x) == EFM_LATENCY_11)                     ||                             \
    ((x) == EFM_LATENCY_12)                     ||                             \
    ((x) == EFM_LATENCY_13)                     ||                             \
    ((x) == EFM_LATENCY_14)                     ||                             \
    ((x) == EFM_LATENCY_15))
	
	
typedef enum en_efm_status
{
    EfmOk           = 0x00u,
    EfmError        = 0x01u,
    EfmBusy         = 0x02u,
    EfmTimeout      = 0x03u,
} en_efm_status_t;

typedef enum en_pe_mode
{
    ReadOnly1       = 0u,   ///< The flash read only.
    SingleProgram   = 1u,   ///< The flash single program.
    SingleProgramRB = 2u,   ///< The flash single program with read back.
    SequenceProgram = 3u,   ///< The flash sequence program.
    SectorErase     = 4u,   ///< The flash sector erase.
    MassErase       = 5u,   ///< The flash mass erase.
    ReadOnly2       = 6u,   ///< The flash read only.
    ReadOnly3       = 7u,   ///< The flash read only.
} en_pe_mode_t;





/*******************************************************************************
 * Global variable definitions ('extern')
 ******************************************************************************/

/*******************************************************************************
  Global function prototypes (definition in C source)
 ******************************************************************************/
void EFM_SetWaitCycle(uint32_t u32Cycle);
void EFM_SetLatency(uint32_t u32Latency);
en_efm_status_t EFM_Unlock(void);
en_efm_status_t EFM_Lock(void);
en_efm_status_t EFM_EraseSector(uint32_t u32Addr);
en_efm_status_t EFM_WriteFlash(uint32_t u32Addr, const uint8_t *pu8WriteBuff, uint32_t u32ByteLength);
en_efm_status_t EFM_ReadFlashWord(uint32_t u32Addr, uint32_t *pu32ReadBuff, uint32_t u32WordLength);
en_efm_status_t EFM_ReadFlashByte(uint32_t u32Addr, uint8_t *pu8ReadBuff, uint32_t u32ByteLength);

en_efm_status_t EFM_WaitForOperationDone(uint32_t u32Timeout);
en_efm_status_t EFM_BlankCheck(uint32_t u32Addr, uint32_t *u32ErrAddr, uint32_t u32Length);

int flash_Init(void);
int flash_Oeminit(void);
int flash_OemRead(uint16_t addr,uint32_t *regval);
int flash_OemWriteStart(void);
int flash_OemWrite(uint16_t regaddr,uint32_t regval);
int flash_OemWriteEnd(void); 
int flash_Write_OEMblock(unsigned int *Buffer);
void flash_Read_OEMblock(unsigned int *Buffer);
int flash_Oemcheck_ifinit(void);

extern uint32_t  Flashdata[2048];


#ifdef __cplusplus
}
#endif

#endif /* __FLASH_H__ */

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
