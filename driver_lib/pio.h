#ifndef _pio_H_
#define _pio_H_
#include "type.h"
#include "hc32_ddl.h"

int  pio_Gpioinit(void);
void pio_GpioRead(uint8 *vals);
void pio_GpioSet(uint8 mask,uint8 vals);

void beep_on(void);
void beep_off(void);
void led_on(void);
void led_off(void);
void rfid_power_on(void);
void rfid_power_off(void);
void ex_power_on(void);//外扩设备电源控制或者复位控制，1正常，0 掉电或者复位
void ex_power_off(void);
void WG_D0_set(uint8  value);//wg d0设置
void WG_D1_set(uint8  value);//wg d1设置
void RS485_set_send(void);//485设置为发送
void RS485_set_rec(void); //485设置为接收
int  get_ipreset_key_value(void); //按下按键为0

#endif








