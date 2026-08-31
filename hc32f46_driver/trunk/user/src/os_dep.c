#include "timer.h"
#include "hc32f46_driver.h"
#include "driverconfig.h"

void sleep_ms(int ms)
{
#if IS_RTOS2_SUPPORT
	osDelay(ms / SYSTEM_TICK_DUR);
#else
	timer_Delay_ms(ms);
#endif	
}


