#include "hc32f46_driver.h"

#define MaxDtsCount 4

unsigned long long ckr_dts[MaxDtsCount];
int ckr_maxdursec;
int ckr_index;

void echr_init(int dursec)
{
	ckr_maxdursec = dursec;
	ckr_index = 0;
}

int echr_istrigger(void)
{
	unsigned long long dt = getSysTick();
	ckr_dts[ckr_index++] = dt;
	
	if (ckr_index+1 == MaxDtsCount)
	{
		if (dt - ckr_dts[0] < ckr_maxdursec * 1000)
			return 1;
		else
		{
			ckr_index = 0;
			return 0;
		}
	}
	else
		return 0;
}
