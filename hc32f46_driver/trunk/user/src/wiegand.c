#include "type.h"
#include "hc32_ddl.h"
#include "timer.h"


//维根第一组
void Wiegand1Data0Clr(void)
{
//  PORT_ResetPortData(PortC, Pin14);
}

void Wiegand1Data0Set(void)
{
// PORT_SetPortData(PortC, Pin14);
}

void Wiegand1Data1Clr(void)
{
//  PORT_ResetPortData(PortC, Pin15);
}

void Wiegand1Data1Set(void)
{
//  PORT_ResetPortData(PortC, Pin15);
}


void WigenPwWithTime(uint8 PwWith)
{
	if((PwWith<4)||(PwWith>20))
	{
		//Time1Delay100us(1);
		timer_Delay_us(100);
	}   
	else
		//Time1Delay10us((PwWith));
	   timer_Delay_us(10*PwWith);
}

void WigenPwCysTime(uint8 PwWith,uint8 PwCyc)
{
	uint16 i=0;
//	uint8 j=0;
	
	if((PwCyc<80)||(PwCyc>2000))
	{
		i=160;
		if((PwWith<4)||(PwWith>20))
		{
			i=i-10;
		}
		else
		{
			i=i-PwWith;
		}	 
	}
	else
	{ 	  
		i= PwCyc;
		if((PwWith<4)||(PwWith>20))
		{
			i=i-10;
		}
		else
		{
			i=i-PwWith;
		}
	}
	
	//Time1Delay10us(i);
	timer_Delay_us(10);
	i=(i/2)/24;
	if(i>0)
	//Time1Delay20us(i);
	timer_Delay_us(20*i);
	
}

void wiegand_init(void)
{									     
//   PORT_SetPortData(PortC, Pin14);
//   PORT_SetPortData(PortC, Pin15);
}

void wiegand_send26(uint8 PwWith,uint8 PwCyc,uint8 *dat)//WigenSel:选择采用那组韦根接口，为0时采用第一组，其他值无效;dat为要发送的24位数据，高字节在前，高位在前
{
	uint32 senddat1=0,senddat2=0,senddat=0;
	uint8 oddflag=0,evenflag=0;
	uint8 count=0,i=0;
	
	senddat1=((dat[0]&0xff)<<24)|((dat[1]&0xff)<<16)|((dat[2]&0xff)<<8);
	senddat2=senddat1;
	
	for(i=0;i<12;i++) //计算偶校验码
	{
		if((senddat1&0x80000000)!=0)
			count++;
		senddat1=senddat1<<1;
	}
	
	if((count%2)!=0)
		evenflag=1;
	
	count=0;
	for(i=0;i<12;i++)  //计算奇校验码
	{
		if((senddat1&0x80000000)!=0)
			count=count+1;
		senddat1=senddat1<<1;
	}
	
	if((count%2)==0)
		oddflag=1;
	senddat=(senddat2&0xffffff00)>>1;
	
	if(evenflag==1)
		senddat=senddat|0x80000000;
	
	if(oddflag==1)
		senddat=senddat|0x00000040;

	for(i=0;i<26;i++)	//发送数据
	{
			if((senddat&0x80000000)!=0)
			{	 
				Wiegand1Data1Clr();
				WigenPwWithTime(PwWith);
				Wiegand1Data1Set();
				WigenPwCysTime(PwWith,PwCyc);
			}
			else
			{
				Wiegand1Data0Clr();
				WigenPwWithTime(PwWith);
				Wiegand1Data0Set();
				WigenPwCysTime(PwWith,PwCyc);
			}
		senddat=senddat<<1;
	}
}

void wiegand_send34(uint8 PwWith,uint8 PwCyc,uint8 *dat)//dat为要发送的32位数据 ，高字节在前，高位在前
{
 uint32 senddat1=0,senddat2=0,senddat=0;
 uint8 oddflag=0,evenflag=0;
 uint8 count=0,i=0;
 senddat1=((dat[0]&0xff)<<24)|((dat[1]&0xff)<<16)|((dat[2]&0xff)<<8)|dat[3];
 senddat2=senddat1;
 for(i=0;i<16;i++)
 {
  if((senddat1&0x80000000)!=0)
    count=count+1;
  senddat1=senddat1<<1;
 }
 if((count%2)!=0)
  evenflag=1;
 count=0;
 for(i=0;i<16;i++)
 {
  if((senddat1&0x80000000)!=0)
    count=count+1;
  senddat1=senddat1<<1;
 }  
 if((count%2)==0)
  oddflag=1;
 senddat=senddat2;
  if(evenflag==1)
  {	 
	  Wiegand1Data1Clr();
	  WigenPwWithTime(PwWith);
	  Wiegand1Data1Set();
	  WigenPwCysTime(PwWith,PwCyc);

  }
  else
  {
      Wiegand1Data0Clr();
	  WigenPwWithTime(PwWith);
	  Wiegand1Data0Set();
	  WigenPwCysTime(PwWith,PwCyc);
  }   
 
 for(i=0;i<32;i++)
 {
     if((senddat&0x80000000)!=0)
     {	 
	   Wiegand1Data1Clr();
	   WigenPwWithTime(PwWith);
	   Wiegand1Data1Set();
	   WigenPwCysTime(PwWith,PwCyc);
     }
     else
     {
       Wiegand1Data0Clr();
	   WigenPwWithTime(PwWith);
	   Wiegand1Data0Set();
	   WigenPwCysTime(PwWith,PwCyc);
     }

   senddat=senddat<<1;
 }
  if(oddflag==1)
  {	 
	  Wiegand1Data1Clr();
	  WigenPwWithTime(PwWith);
	  Wiegand1Data1Set();
	  WigenPwCysTime(PwWith,PwCyc);
  }
  else
  {
      Wiegand1Data0Clr();
	  WigenPwWithTime(PwWith);
	  Wiegand1Data0Set();
	  WigenPwCysTime(PwWith,PwCyc);
  }  
} 
void wiegand_send66(uint8 PwWith,uint8 PwCyc,uint8 *dat)//dat为要发送的64位数据 ，高字节在前，高位在前
{
 uint32 senddat1=0,senddat2=0,senddat[2]={0,0};
 uint8 oddflag=0,evenflag=0;
 uint8 count=0,i=0;
 senddat1=((dat[0]&0xff)<<24)|((dat[1]&0xff)<<16)|((dat[2]&0xff)<<8)|dat[3];
 senddat2=((dat[4]&0xff)<<24)|((dat[5]&0xff)<<16)|((dat[6]&0xff)<<8)|dat[7];
 senddat[0]=senddat1;
 senddat[1]=senddat2;
 for(i=0;i<32;i++)
 {
  if((senddat1&0x80000000)!=0)
    count=count+1;
  senddat1=senddat1<<1;
 }
 if((count%2)!=0)
  evenflag=1;
 count=0;
 for(i=0;i<32;i++)
 {
  if((senddat2&0x80000000)!=0)
    count=count+1;
  senddat2=senddat2<<1;
 }  
 if((count%2)==0)
  oddflag=1;
 
    if(evenflag==1)
    {	 
	  Wiegand1Data1Clr();
	  WigenPwWithTime(PwWith);
	  Wiegand1Data1Set();
	  WigenPwCysTime(PwWith,PwCyc);
    }
    else
    {
       Wiegand1Data0Clr();
	   WigenPwWithTime(PwWith);
	   Wiegand1Data0Set();
	   WigenPwCysTime(PwWith,PwCyc);
    }   

 for(i=0;i<32;i++)
 {
     if((senddat[0]&0x80000000)!=0)
     {	 
	   Wiegand1Data1Clr();
	   WigenPwWithTime(PwWith);
	   Wiegand1Data1Set();
	   WigenPwCysTime(PwWith,PwCyc);
     }
     else
     {
       Wiegand1Data0Clr();
	   WigenPwWithTime(PwWith);
	   Wiegand1Data0Set();
	   WigenPwCysTime(PwWith,PwCyc);
     }
   senddat[0]=senddat[0]<<1;
 }
 for(i=0;i<32;i++)
 {
     if((senddat[1]&0x80000000)!=0)
     {	 
	   Wiegand1Data1Clr();
	   WigenPwWithTime(PwWith);
	   Wiegand1Data1Set();
	   WigenPwCysTime(PwWith,PwCyc);
     }
     else
     {
       Wiegand1Data0Clr();
	   WigenPwWithTime(PwWith);
	   Wiegand1Data0Set();
	   WigenPwCysTime(PwWith,PwCyc);
     }

   senddat[1]=senddat[1]<<1;
 }
    if(oddflag==1)
    {	 
	  Wiegand1Data1Clr();
	  WigenPwWithTime(PwWith);
	  Wiegand1Data1Set();
	  WigenPwCysTime(PwWith,PwCyc);
    }
    else
    {
      Wiegand1Data0Clr();
	  WigenPwWithTime(PwWith);
	  Wiegand1Data0Set();
	  WigenPwCysTime(PwWith,PwCyc);
    }	  
}

