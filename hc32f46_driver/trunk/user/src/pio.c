#include  "pio.h"


void RS485_set_send(void)//485设置为发送
{
    //PORT_SetPortData(PortB, Pin15);
}
void RS485_set_rec(void)//485设置为接收
{
   // PORT_ResetPortData(PortB, Pin15);
}

void rfid_power_on(void)
{
   // PORT_ResetPortData(PortB, Pin02);
}
void rfid_power_off(void)
{
   // PORT_SetPortData(PortB, Pin02);
}

void ex_power_on(void)//外扩设备电源控制或者复位控制，1正常，0 掉电或者复位
{
   // PORT_SetPortData(PortC, Pin13);
}
void ex_power_off(void)
{
   // PORT_ResetPortData(PortC, Pin13);
}

void WG_D1_set(uint8  value)//wg d1设置
{
   // if(value==1)
   //     PORT_SetPortData(PortC, Pin14);
   // else if(value==0)
   //     PORT_ResetPortData(PortC, Pin14);
}
void WG_D0_set(uint8  value)//wg d0设置
{
   // if(value==1)
   //     PORT_SetPortData(PortC, Pin15);
   // else if(value==0)
   //     PORT_ResetPortData(PortC, Pin15);
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
	  LED_Key_Switch(1);
    PORT_SetPortData(PortC, Pin15);
}
void led_off(void)
{
	  LED_Key_Switch(1);
    PORT_ResetBits(PortC, Pin15);
}

int get_ipreset_key_value(void)
{
     LED_Key_Switch(0);
		 return PORT_GetBit(PortC,Pin15);
}


int pio_Gpioinit(void)
{
    PORT_ResetPortData(PortB, Pin04); // out1  0
    PORT_ResetPortData(PortB, Pin03); // out2  0
    PORT_ResetPortData(PortC, Pin15); // out3  0
    PORT_ResetPortData(PortA, Pin15); // out4  0
    return 0;
}


void gpo_set(uint8 gpoid, uint8 state)
{
    if(gpoid == 1)//RLED
    {
        if(state == 1)	          
			  PORT_SetPortData(PortB, Pin04);
        else      				      
			  PORT_ResetPortData(PortB, Pin04);
    }
    if(gpoid == 2)//GLED
    {
        if (state == 1)	  
			  PORT_SetPortData(PortB, Pin03);
        else      				      
			  PORT_ResetPortData(PortB, Pin03);
    }
    if(gpoid == 3)//BLED
    {
        if (state == 1)	 
			  PORT_SetPortData(PortA, Pin15);  // O3--PC15-HC32HPM
        else      				     
			  PORT_ResetPortData(PortA, Pin15);// O3--PC15-HC32HPM
    }
    if(gpoid == 4)
    {
        if (state == 1)	
			  PORT_SetPortData(PortC, Pin15);
        else      				  
			  PORT_ResetPortData(PortC, Pin15);
    }
	 else if(gpoid == 5)
	 {
		 if (state == 0)
			beep_off();
		 else
			beep_on();
	 }
}

uint8 gpi_get(uint8 gpoid)
{
	uint8 state;
	pio_GpioRead(&state);
	return (state >> (gpoid-1)) & 0x01;
}

uint8 gpi_get_all()
{
	uint8 state;
	pio_GpioRead(&state);
	return state;
}


void pio_GpioRead(uint8 *vals) //读输入IO口状态 ，IN1的值存放在vals的bit0位，IN2的值存放在vals的bit1位,IN3的值存放在vals的bit2位,IN4的值存放在vals的bit3位,
{
    *vals=PORT_GetBit(PortB,Pin05);
}

void pio_GpioSet(uint8 mask,uint8 vals)//设置输出IO口状态，mask的bit0位表示输出IO1,bit1表示输出IO2，
{   //当bit0或bit1值为1时才表示要设置对应的IO口，设置的值为对应的vals的bit0与bit1的值

    if((mask&0x01)==1)
    {
        if((vals&0x01)==1)	        PORT_SetPortData(PortB, Pin04);
        else      				          PORT_ResetPortData(PortB, Pin04);
    }
    if(((mask&0x02)>>1)==1)
    {
        if (((vals&0x02)>>1)==1)	  PORT_SetPortData(PortB, Pin03);
        else      				          PORT_ResetPortData(PortB, Pin03);
    }
    if(((mask&0x04)>>2)==1)
    {
        if (((vals&0x04)>>2)==1)	  PORT_SetPortData(PortA, Pin15);  // O3--PC15-HC32HPM
        else      				          PORT_ResetPortData(PortA, Pin15);// O3--PC15-HC32HPM
    }
    if(((mask&0x08)>>3)==1)
    {
        if(((vals&0x08)>>3)==1)	      PORT_SetPortData(PortC, Pin15);
        else      				            PORT_ResetPortData(PortC, Pin15);
    }
}

