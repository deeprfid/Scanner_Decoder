#include "flash.h"
#include "hc32_ddl.h"
#include "type.h"
#include "hc32f46_driver.h"
#include "driverconfig.h"

#define FLASH_SECTOR_SIZE      0x2000ul
#define FLASH_SIZE            (64u * FLASH_SECTOR_SIZE)
#define RAM_SIZE               0x2F000ul
	

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



void EFM_SetLatency(uint32_t u32Latency)
{
    //  DDL_ASSERT(IS_VALID_FLASH_LATENCY(u32Latency));

    M4_EFM->FRMC_f.FLWT = u32Latency;
}



void EFM_SetWaitCycle(uint32_t u32Cycle)
{
    uint32_t u32Temp;

    /* Set flash wait cycle. */
    /* Unlock flash. */
    EFM_Unlock();
    /* Set wait cycle. */
    u32Temp  = M4_EFM->FRMC;
    u32Temp &= ~(0xFul << 4u);
    u32Temp |= (u32Cycle << 4u);
    M4_EFM->FRMC = u32Temp;
    /* Lock flash. */
    EFM_Lock();
}

/**
 ******************************************************************************
 ** \brief  Unlock the FLASH control register access
 **
 ** \param  None
 **
 ** \retval Return value Here
 ******************************************************************************/
en_efm_status_t EFM_Unlock(void)
{
    en_efm_status_t enStatus = EfmOk;

    if (0u == M4_EFM->FAPRT)
    {
        /* Authorize the EFM Registers access */
        M4_EFM->FAPRT = 0x0123;
        M4_EFM->FAPRT = 0x3210;

        /* Verify Flash is unlocked */
        if (0u == M4_EFM->FAPRT)
        {
            enStatus = EfmError;
        }
    }

    return enStatus;
}

/**
 ******************************************************************************
 ** \brief  Locks the EFM control register access
 **
 ** \param  None
 **
 ** \retval EFM Status
 ******************************************************************************/
en_efm_status_t EFM_Lock(void)
{
    en_efm_status_t enStatus = EfmOk;

    if (1u == M4_EFM->FAPRT)
    {
        /* Authorize the EFM Registers access */
        M4_EFM->FAPRT = 0;

        /* Verify Flash is locked */
        if (0u == M4_EFM->FAPRT)
        {
            enStatus = EfmError;
        }
    }

    return enStatus;
}

en_efm_status_t EFM_WaitForOperationDone(uint32_t u32Timeout)
{
    volatile uint32_t u32Delay = 0;

    /* Wait for the EFM operation to complete by polling on BUSY flag to be
       reset. Even if the EFM operation fails, the BUSY flag will be reset and
       an error flag will be set */
    while ((1u != M4_EFM->FSR_f.RDY))// && (1u != M4_EFM->FSR_f.OPTEND))
    {
        if (u32Timeout == u32Delay++)
        {
            return EfmTimeout;
        }
    }

    /* Check FLASH End of Operation flag  */
    if (1u == M4_EFM->FSR_f.OPTEND)
    {
        /* Clear FLASH End of Operation pending bit */
        M4_EFM->FSCLR_f.OPTENDCLR = 1u;
    }

    if (0 != !!(M4_EFM->FSR & 0x3Fu))
    {
        M4_EFM->FSCLR |= 0x3Fu;
        return EfmError;
    }

    return EfmOk;
}

int flash_bytes_write(uint32 addr,void *buf, uint16 len)
{
    int i;
	 uint8 *buf_ = buf;
    en_efm_status_t enStatus;
    __IO uint32_t *io32Flash = (uint32 *)addr;
//	TRACE("aa\n");
	__disable_irq();
	if (flash_sector_erase(addr) != 0)
	{
		__enable_irq();
		return -1;
	}
    EFM_Unlock();
//	TRACE("bb\n");
    enStatus = EFM_WaitForOperationDone(1000);
//	TRACE("cc\n");
    if (enStatus != EfmOk)
    {
        EFM_Lock();
		  __enable_irq();
//		 TRACE("flash_Bytes_Write error:%d\n", enStatus);
        return -1;
    }
//	 TRACE("dd\n");
    M4_EFM->FSCLR = (uint32)0x3F;
    M4_EFM->FWMC_f.PEMODE = 0x1u;
    M4_EFM->FWMC_f.PEMOD  = SingleProgramRB;
//	 TRACE("ee\n");
    for (i = 0u; i <len / 4; i++)
    {
        *io32Flash = GetNumU32(buf_+i*4);
        while(1 != M4_EFM->FSR_f.RDY);
        if (1 == M4_EFM->FSR_f.PGMISMTCH)
        {
            enStatus = EfmError;
            M4_EFM->FWMC_f.PEMOD  = ReadOnly1;
            M4_EFM->FWMC_f.PEMODE = 0x0u;
            EFM_Lock();
			   __enable_irq();
//			  TRACE("flash_Bytes_Write error:%d\n", enStatus);
            return -1;
        }
        io32Flash++;
        M4_EFM->FSCLR |= (uint32)EFM_FLAG_EOP;
    }
//	 TRACE("ff\n");
    /* Set flash read only. */
    M4_EFM->FWMC_f.PEMOD  = ReadOnly1;
    M4_EFM->FWMC_f.PEMODE = 0x0u;

    EFM_Lock();
//	  TRACE("gg\n");
	 __enable_irq();
//	  TRACE("hh\n");
	 return 0;
}


void flash_bytes_read(uint32 addr,void *buf,uint16 len)
{
    int i;
    EFM_Unlock();
	 uint32 val;
	 uint8 *buf_ = buf;
    for(i = 0; i < len/4; i++)
    {
		 val = *(( uint32  * ) (addr + i * 4));
		 SetNumU32(buf_+i*4, val);
    }
    EFM_Lock();
}

/**
 ******************************************************************************
 ** \brief  Erase flash block.
 **
 ** \param  Parameters Here
 **
 ** \retval Return value Here
 ******************************************************************************/
int flash_sector_erase(uint32 addr)
{
    en_efm_status_t enStatus;
    EFM_Unlock();

    enStatus = EFM_WaitForOperationDone(1000);
    addr  = (addr / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE + 4u;
    if (EfmOk == enStatus)
    {
//		 printf("flash_sector_erase: if (EfmOk == enStatus)\n");
        M4_EFM->FWMC_f.PEMODE = 1u;                 //Enable flash erase and read
        M4_EFM->FWMC_f.PEMOD  = SectorErase;        //Set block erase mode

        *(volatile uint32_t *)(addr) = 0xFFFFFFFF;

        enStatus = EFM_WaitForOperationDone(1000);
//			printf("EFM_WaitForOperationDone:%d\n", enStatus);
        M4_EFM->FWMC_f.PEMOD = ReadOnly1;
        M4_EFM->FWMC_f.PEMODE = 0u;
    }
//	 else
//		 printf("flash_sector_erase: if (EfmOk != enStatus):%d\n", enStatus);

    EFM_Lock();
	 if (enStatus != EfmOk)
	 {
//		 TRACE("flash_Sector_Erase error:%d\n", enStatus);
		 return -1;
	 }
	 return 0;
}

