
#ifndef _timer_H_
#define _timer_H_

#include "hc32_ddl.h"
#include  "type.h"

#define  ONEMSC   82
#define  ONEUSC   21


uint64_t  getSysTick(void);//获取系统时间，单位ms
void timer_ini(void);
void timer4_ini(void);
void  timer_Init(void);//定时计数器初始化函数，上电后的初始化配置函数,配置成功返回0，不成功返回1
void timer_Get_msctr(unsigned int* timestamp);////读取间隔必须大于1us,否则不准，该函数为获取模块时间值，获取时间值存放在timestamp，单位ms，第一次获取时为0，
                                              //后面获取时即是与第一次获取相隔的时间值，当timestamp=0xffff ffff后即重新从0开始
void timer_Get_lltimer(unsigned int* timestamp); //获取目前计时器的计数值，计数值放入timestamp
unsigned int timer_Diff_lltimer(unsigned int lltimer_stamp);//返回值为从计时器的计数值为lltimer_stamp开始到现在计时器又经历了多少计数值
															//计数值每加1代表经过多少时间要备注一下
//unsigned int timer_Diff_us(unsigned int lltimer_stamp);//返回值为计时器的计数值为lltimer_stamp时到现在所经历的时间，单位微妙
unsigned int timer_Diff_ms(unsigned int lltimer_stamp);//返回值为计时器的计数值为lltimer_stamp时到现在所经历的时间，单位毫妙

void timer_Delay_us(unsigned int us);	//微妙级延时，调用后等待延时结束才退出该函数
void timer_Delay_ms(unsigned int ms);	//毫秒级延时，调用后等待延时结束才退出该函数

#endif

