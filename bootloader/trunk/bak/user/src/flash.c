
#include "flash.h"
#include "hc32_common.h"
#include "hc32_ddl.h"
#include "type.h"

uint32  Flashwriteaddr;
uint32  Flashdata[2048];
uint8   Flashwritestart=0;
uint16  Flashpagenum;



int flash_Init(void)
{
    int retval = 0;
    int flag=0;
    Flashwritestart=0;
    flag=flash_Oemcheck_ifinit();
    if(flag==1)
    {
        retval=1;
    }
    else if(flag!=0)
    {
        retval = flash_Oeminit();
        if(retval!=0)
        {
            return retval;
        }
        flash_OemWriteStart();
        flash_OemWrite(2047,0xa5a55a5a);
        flash_OemWriteEnd();
    }

    return retval;
}

int flash_Oeminit()
{
    uint32 oemva=0;
    uint16 i;

    flash_OemWriteStart();
    SWDT_RefreshCounter();
    for(i=0; i<(2048); i++)
    {
        if(flash_OemWrite(i,oemva)!=0) return 1;
    }
    flash_OemWriteEnd();
    SWDT_RefreshCounter();
    return 0;
}



int flash_Oemcheck_ifinit(void)
{
    uint32 dat[2];
    if(flash_OemRead(2047,dat)!=0)
    {
        return 1;
    }

    if(dat[0]!=0xa5a55a5a)
    {
        flash_OemRead(2047,dat);
        if(dat[0]!=0xa5a55a5a)
            return -1;
        else
            return 0;
    }
    else
        return 0;

}


int flash_OemWriteStart()
{
    Flashwritestart=1;
    Flashpagenum=0;
    return 0;
}



int flash_OemWrite(uint16 regaddr,uint32 regval)
{

    if(regaddr>=2048) return 1;	//·Ç·¨µØÖ·
    if(Flashwritestart==1)
    {
        flash_Read_OEMblock(Flashdata);
        Flashdata[regaddr]=regval;
        Flashwritestart=0;
    }
    else
    {
        Flashdata[regaddr]=regval;
    }
    return 0;
}




int flash_OemWriteEnd(void)
{

    USART_FuncCmd(M4_USART1, UsartRxInt, Disable);
    USART_FuncCmd(M4_USART3, UsartRxInt, Disable);
    if(flash_Write_OEMblock(Flashdata)==1)
    {
        USART_FuncCmd(M4_USART1, UsartRxInt, Enable);
        USART_FuncCmd(M4_USART3, UsartRxInt, Enable);
        return 1;
    }
    USART_FuncCmd(M4_USART1, UsartRxInt, Enable);
    USART_FuncCmd(M4_USART3, UsartRxInt, Enable);
    return 0;
}


int flash_Write_OEMblock(unsigned int *Buffer)
{
    unsigned int i;
    en_efm_status_t enStatus;
    __IO uint32_t *io32Flash = (uint32_t *)OEMADDR;
    EFM_EraseSector(OEMADDR);
    EFM_Unlock();
    enStatus = EFM_WaitForOperationDone(1000);
    if (enStatus != EfmOk)
    {
        EFM_Lock();
        return 1;
    }
    M4_EFM->FSCLR = (uint32_t)0x3F;
    M4_EFM->FWMC_f.PEMODE = 0x1u;
    M4_EFM->FWMC_f.PEMOD  = SingleProgramRB;
    SWDT_RefreshCounter();
    for (i = 0u; i <2048; i++)
    {
        *io32Flash = Buffer[i];
        while(1 != M4_EFM->FSR_f.RDY);
        if (1 == M4_EFM->FSR_f.PGMISMTCH)
        {
            enStatus = EfmError;
            M4_EFM->FWMC_f.PEMOD  = ReadOnly1;
            M4_EFM->FWMC_f.PEMODE = 0x0u;
            EFM_Lock();
            return 1;
        }
        io32Flash++;
        M4_EFM->FSCLR |= (uint32_t)EFM_FLAG_EOP;
    }
    SWDT_RefreshCounter();
    /* Set flash read only. */
    M4_EFM->FWMC_f.PEMOD  = ReadOnly1;
    M4_EFM->FWMC_f.PEMODE = 0x0u;

    EFM_Lock();
    return 0;
}


void flash_Read_OEMblock(unsigned int *Buffer)
{
    unsigned int i,j=0;
    EFM_Unlock();
    SWDT_RefreshCounter();
    for( i = 0; i < 2048; i++ )
    {
        Buffer[ i ] = *( ( unsigned int  * ) (OEMADDR+ j)  ) ;
        j+=4;
    }
    EFM_Lock();
    SWDT_RefreshCounter();

}


int flash_OemRead(uint16 addr,uint32 *regval)
{   unsigned int  *Flash_Address;
    EFM_Unlock();
    Flash_Address = ( unsigned int  * )((addr*4+OEMADDR)&0xffffffff);
    *regval= *Flash_Address;
    EFM_Lock();
    return 0;
}



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

/**
 ******************************************************************************
 ** \brief  Erase flash block.
 **
 ** \param  Parameters Here
 **
 ** \retval Return value Here
 ******************************************************************************/
en_efm_status_t EFM_EraseSector(uint32_t u32Addr)
{
    en_efm_status_t enStatus;

    EFM_Unlock();

    enStatus = EFM_WaitForOperationDone(1000);
    u32Addr  = (u32Addr / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE + 4u;
    if (EfmOk == enStatus)
    {
        M4_EFM->FWMC_f.PEMODE = 1u;                 //Enable flash erase and read
        M4_EFM->FWMC_f.PEMOD  = SectorErase;        //Set block erase mode

        *(volatile uint32_t *)(u32Addr) = 0xFFFFFFFF;

        enStatus = EFM_WaitForOperationDone(1000);

        M4_EFM->FWMC_f.PEMOD = ReadOnly1;
        M4_EFM->FWMC_f.PEMODE = 0u;
    }

    EFM_Lock();

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

