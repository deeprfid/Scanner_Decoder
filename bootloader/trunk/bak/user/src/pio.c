#include  "pio.h"


void RS485_set_send(void)//485设置为发送
{
    PORT_SetPortData(PortB, Pin15);
}
void RS485_set_rec(void)//485设置为接收
{
    PORT_ResetPortData(PortB, Pin15);
}

void rfid_power_on(void)
{
    PORT_ResetPortData(PortB, Pin02);
}
void rfid_power_off(void)
{
    PORT_SetPortData(PortB, Pin02);
}

void ex_power_on(void)//外扩设备电源控制或者复位控制，1正常，0 掉电或者复位
{
    PORT_SetPortData(PortC, Pin13);
}
void ex_power_off(void)
{
    PORT_ResetPortData(PortC, Pin13);
}

void WG_D1_set(uint8  value)//wg d1设置
{
    if(value==1)
        PORT_SetPortData(PortC, Pin14);
    else if(value==0)
        PORT_ResetPortData(PortC, Pin14);
}
void WG_D0_set(uint8  value)//wg d0设置
{
    if(value==1)
        PORT_SetPortData(PortC, Pin15);
    else if(value==0)
        PORT_ResetPortData(PortC, Pin15);
}


void beep_on(void)
{
    PORT_SetPortData(PortA, Pin10);
}
void beep_off(void)
{
    PORT_ResetPortData(PortA, Pin10);
}

void led_on(void)
{
    PORT_ResetPortData(PortB, Pin03);
}
void led_off(void)
{
    PORT_SetPortData(PortB, Pin03);
}

int get_ipreset_key_value(void)
{
    if(PORT_GetBit(PortA, Pin08)==1) return 1;
    else return 0;
}


int pio_Gpioinit(void)
{
    PORT_ResetPortData(PortB, Pin05); // out1  0
    PORT_ResetPortData(PortB, Pin08); // out2  0
    PORT_ResetPortData(PortB, Pin09); // out3  0
    PORT_ResetPortData(PortH, Pin02); // out4  0
    return 0;
}


void pio_GpioRead(uint8 *vals) //读输入IO口状态 ，IN1的值存放在vals的bit0位，IN2的值存放在vals的bit1位,IN3的值存放在vals的bit2位,IN4的值存放在vals的bit3位,
{
    *vals=( PORT_GetBit(PortA, Pin13)| (PORT_GetBit(PortA,Pin14)<<1) | (PORT_GetBit(PortA,Pin15)<<2) | (PORT_GetBit(PortB,Pin04)<<3));
}

void pio_GpioSet(uint8 mask,uint8 vals)//设置输出IO口状态，mask的bit0位表示输出IO1,bit1表示输出IO2，
{   //当bit0或bit1值为1时才表示要设置对应的IO口，设置的值为对应的vals的bit0与bit1的值

    if((mask&0x01)==1)
    {
        if((vals&0x01)==1)	          PORT_SetPortData(PortB, Pin05);
        else      				      PORT_ResetPortData(PortB, Pin05);
    }
    if(((mask&0x02)>>1)==1)
    {
        if (((vals&0x02)>>1)==1)	  PORT_SetPortData(PortB, Pin08);
        else      				      PORT_ResetPortData(PortB, Pin08);
    }
    if(((mask&0x04)>>2)==1)
    {
        if (((vals&0x04)>>2)==1)	  PORT_SetPortData(PortB, Pin09);
        else      				      PORT_ResetPortData(PortB, Pin09);
    }
    if(((mask&0x08)>>3)==1)
    {
        if(((vals&0x08)>>3)==1)	      PORT_SetPortData(PortH, Pin02);
        else      				      PORT_ResetPortData(PortH, Pin02);
    }
}

