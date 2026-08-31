#include "timer.h"
#include "hc32_ddl.h"
#include "irq.h"
uint8    Timerfirstreadmtime;
uint32	 Timermsvalue;
uint32	 Timermslastva;

uint64_t  getSysTick(void)//获取系统时间，单位ms
{
    uint32   tcount = TIMERA_GetCurrCount(M4_TMRA4);
		return ( (512*gSysTickCnt)+(tcount/80) );
}


//误差1ms,返回TIMESTAMP为所经历的时间，最长是14小时后重新复位，但是必须读取一次后下次的读取时间不超过14小时
//读取间隔必须大于1us,否则不准
void timer_Get_msctr(unsigned int* timestamp)
{
    uint32 mscurrva1,mscurrva2;
    uint8  tmp=5;
    if(Timerfirstreadmtime==1)
    {   TIMERA_Cmd(M4_TMRA1, Disable);
        TIMERA_Cmd(M4_TMRA2, Disable);
        TIMERA_SetCurrCount(M4_TMRA1,0);
        TIMERA_SetCurrCount(M4_TMRA2,0);
        TIMERA_Cmd(M4_TMRA2, Enable);
        TIMERA_Cmd(M4_TMRA1, Enable);
        *timestamp=0;
        Timermslastva=0;
        Timermsvalue=0;
        Timerfirstreadmtime=0;
    }
    else
    {
        do
        {
            mscurrva1=( (TIMERA_GetCurrCount(M4_TMRA2)<<16)| TIMERA_GetCurrCount(M4_TMRA1) );
            mscurrva2=( (TIMERA_GetCurrCount(M4_TMRA2)<<16)| TIMERA_GetCurrCount(M4_TMRA1) );
            tmp--;
        }
        while( (mscurrva1!=mscurrva2) && (tmp>0) );
        if(Timermslastva>mscurrva1)
        {
            Timermsvalue+=( (mscurrva1 +  (0xFFFFFFFF - Timermslastva+1) )/ONEMSC);
        }
        else
            Timermsvalue+=( (mscurrva1 - Timermslastva)/ONEMSC );
        *timestamp=Timermsvalue;
        Timermslastva=mscurrva1;
    }
}


void timer_Get_lltimer(unsigned int* timestamp)//获取目前计时器的计数值，计数值放入timestamp
{
    uint32 mscurrva1,mscurrva2;
    uint8  tmp=5;
    do
    {
        mscurrva1=( (TIMERA_GetCurrCount(M4_TMRA2)<<16)| TIMERA_GetCurrCount(M4_TMRA1) );
        mscurrva2=( (TIMERA_GetCurrCount(M4_TMRA2)<<16)| TIMERA_GetCurrCount(M4_TMRA1) );
        tmp--;
    }
    while( (mscurrva1!=mscurrva2) && (tmp>0) );

    *timestamp=mscurrva1;
}



//返回值为从计时器的计数值为lltimer_stamp开始到现在计时器又经历了多少计数值,82个计数值表示1ms
unsigned int timer_Diff_lltimer(unsigned int lltimer_stamp)
{
    uint32 mscurrva1,mscurrva2;
    uint8  tmp=5;
    uint32 msdiff;
    do
    {
        mscurrva1=( (TIMERA_GetCurrCount(M4_TMRA2)<<16)| TIMERA_GetCurrCount(M4_TMRA1) );
        mscurrva2=( (TIMERA_GetCurrCount(M4_TMRA2)<<16)| TIMERA_GetCurrCount(M4_TMRA1) );
        tmp--;
    }
    while( (mscurrva1!=mscurrva2) && (tmp>0) );
    if(lltimer_stamp>mscurrva1)
        msdiff=(mscurrva1 +  (0xFFFFFFFF - lltimer_stamp+1));
    else msdiff=(mscurrva1 - lltimer_stamp);
    return msdiff;
}

unsigned int timer_Diff_ms(unsigned int lltimer_stamp)//返回值为计时器的计数值为lltimer_stamp时到现在所经历的时间，单位毫妙
{
    uint32 mscurrva1,mscurrva2;
    uint8  tmp=5;
    uint32 msdiff;
    do
    {
        mscurrva1=( (TIMERA_GetCurrCount(M4_TMRA2)<<16)| TIMERA_GetCurrCount(M4_TMRA1) );
        mscurrva2=( (TIMERA_GetCurrCount(M4_TMRA2)<<16)| TIMERA_GetCurrCount(M4_TMRA1) );
        tmp--;
    }
    while( (mscurrva1!=mscurrva2) && (tmp>0) );
    if(lltimer_stamp>mscurrva1)
        msdiff=(mscurrva1 +  (0xFFFFFFFF - lltimer_stamp+1));
    else msdiff=(mscurrva1 - lltimer_stamp);
    return (msdiff/ONEMSC);
}


void timer_Delay_us(unsigned int us)	//微妙级延时，调用后等待延时结束才退出该函数,最长0xffffffff us
{
    uint32 tu;
    if (us/1000>0)
    {
        for(tu=0; tu<us/1000; tu++)
        {   TIMERA_Cmd(M4_TMRA3, Disable);
            TIMERA_SetCurrCount(M4_TMRA3,0);
            TIMERA_Cmd(M4_TMRA3, Enable);
            while (TIMERA_GetCurrCount(M4_TMRA3)<(1000*ONEUSC) )
            {
                SWDT_RefreshCounter();
            }
        }
        if(us%1000>0)
        {   TIMERA_Cmd(M4_TMRA3, Disable);
            TIMERA_SetCurrCount(M4_TMRA3,0);
            TIMERA_Cmd(M4_TMRA3, Enable);
            while (TIMERA_GetCurrCount(M4_TMRA3)<(us%1000)*ONEUSC)
            {
                SWDT_RefreshCounter();
            }
        }
    }
    else
    {   TIMERA_Cmd(M4_TMRA3, Disable);
        TIMERA_SetCurrCount(M4_TMRA3,0);
        TIMERA_Cmd(M4_TMRA3, Enable);
        while (TIMERA_GetCurrCount(M4_TMRA3)<us*ONEUSC)
        {
            SWDT_RefreshCounter();
        }
    }
}


void timer_Delay_ms(unsigned int ms)	//毫秒级延时，调用后等待延时结束才退出该函数,最长0xffffffff ms
{
    uint32 tm;

    if (ms/3>0) //>3ms
    {
        for(tm=0; tm<ms/3; tm++)
        {   TIMERA_Cmd(M4_TMRA3, Disable);
            TIMERA_SetCurrCount(M4_TMRA3,0);
            TIMERA_Cmd(M4_TMRA3, Enable);
            while (TIMERA_GetCurrCount(M4_TMRA3)<(3000*ONEUSC) )
            {
//                SWDT_RefreshCounter();
            }
        }
        if(ms%3>0)
        {   TIMERA_Cmd(M4_TMRA3, Disable);
            TIMERA_SetCurrCount(M4_TMRA3,0);
            TIMERA_Cmd(M4_TMRA3, Enable);
            while (TIMERA_GetCurrCount(M4_TMRA3)<(ms%3)*1000*ONEUSC)
            {
//                SWDT_RefreshCounter();
            }
        }
    }
    else
    {   TIMERA_Cmd(M4_TMRA3, Disable);
        TIMERA_SetCurrCount(M4_TMRA3,0);
        TIMERA_Cmd(M4_TMRA3, Enable);
        while (TIMERA_GetCurrCount(M4_TMRA3)<(ms*1000*ONEUSC) )
        {
//            SWDT_RefreshCounter();
        }
    }
}




void timer_ini(void)
{
    //timera1,timera2用在ms计时，timera3用于us延时
    stc_timera_base_init_t stcTimeraInit;
    stc_timera_orthogonal_coding_init_t stcTimeraCondingInit;

    /* configuration structure initialization */
    MEM_ZERO_STRUCT(stcTimeraInit);
    MEM_ZERO_STRUCT(stcTimeraCondingInit);

    /* Configuration peripheral clock */
    PWC_Fcg2PeriphClockCmd(PWC_FCG2_PERIPH_TIMA1 | PWC_FCG2_PERIPH_TIMA2 | PWC_FCG2_PERIPH_TIMA3, Enable);

    /* Configuration timera unit 1 structure */
    stcTimeraInit.enClkDiv = TimeraPclkDiv1024;
    stcTimeraInit.enCntMode = TimeraCountModeSawtoothWave;  //锯齿波
    stcTimeraInit.enCntDir = TimeraCountDirUp;
    stcTimeraInit.enSyncStartupEn = Disable;  //同步启动关闭
    stcTimeraInit.u16PeriodVal = 0xffff;
    TIMERA_BaseInit(M4_TMRA1, &stcTimeraInit);

    /* Configure timera uint 2 structure */
    stcTimeraInit.u16PeriodVal = 0xffff;
    TIMERA_BaseInit(M4_TMRA2, &stcTimeraInit);
    stcTimeraCondingInit.enIncAnotherUnitOverflowEn = Enable; //定时器1溢出作为输入
    TIMERA_OrthogonalCodingInit(M4_TMRA2, &stcTimeraCondingInit);

    /* Configuration timera unit 3 structure */
    stcTimeraInit.enClkDiv = TimeraPclkDiv4;
    stcTimeraInit.enCntMode = TimeraCountModeSawtoothWave;  //锯齿波
    stcTimeraInit.enCntDir = TimeraCountDirUp;
    stcTimeraInit.enSyncStartupEn = Disable;  //同步启动关闭
    stcTimeraInit.u16PeriodVal = 0xffff;
    TIMERA_BaseInit(M4_TMRA3, &stcTimeraInit);

    /* Timera unit 1 and unit 2 startup */ //先开timer2 在开timer1
    TIMERA_Cmd(M4_TMRA2, Enable);
    TIMERA_Cmd(M4_TMRA1, Enable);
    TIMERA_Cmd(M4_TMRA3, Enable);

}

void timer4_ini(void)//512ms中断一次，用于ms 长时间计时
{
    stc_timera_base_init_t stcTimeraInit;
    stc_timera_orthogonal_coding_init_t stcTimeraCondingInit;
    stc_irq_regi_conf_t stcIrqRegiConf;

    /* configuration structure initialization */
    MEM_ZERO_STRUCT(stcTimeraInit);
    MEM_ZERO_STRUCT(stcTimeraCondingInit);
    MEM_ZERO_STRUCT(stcIrqRegiConf);

    /* Configuration peripheral clock */
    PWC_Fcg2PeriphClockCmd(PWC_FCG2_PERIPH_TIMA4, Enable);

    /* Configuration timera unit 4 structure */
    stcTimeraInit.enClkDiv = TimeraPclkDiv1024;
    stcTimeraInit.enCntMode = TimeraCountModeSawtoothWave;  //锯齿波
    stcTimeraInit.enCntDir = TimeraCountDirUp;
    stcTimeraInit.enSyncStartupEn = Disable;  //同步启动关闭
    stcTimeraInit.u16PeriodVal = (42000-1);  //512ms 溢出中断
    TIMERA_BaseInit(M4_TMRA4, &stcTimeraInit);

    stcIrqRegiConf.enIntSrc = INT_TMRA4_OVF;
    stcIrqRegiConf.enIRQn = Int009_IRQn;
    stcIrqRegiConf.pfnCallback = &TimeraUnit4_IrqCallback;
    enIrqRegistration(&stcIrqRegiConf);
    NVIC_ClearPendingIRQ(stcIrqRegiConf.enIRQn);
    NVIC_SetPriority(stcIrqRegiConf.enIRQn, DDL_IRQ_PRIORITY_15);
    NVIC_EnableIRQ(stcIrqRegiConf.enIRQn);
    TIMERA_IrqCmd(M4_TMRA4, TimeraIrqOverflow, Enable);
    TIMERA_Cmd(M4_TMRA4, Enable);

}

void timer_Init(void)
{

    unsigned int timecfirstread;
    Timerfirstreadmtime=1;
    Timermsvalue=0;
    Timermslastva=0;
    timer_ini();
    timer4_ini();
    timer_Get_msctr(&timecfirstread); //第一次获取时间
}

