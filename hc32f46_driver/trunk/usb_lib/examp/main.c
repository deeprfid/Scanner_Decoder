/*******************************************************************************
 * Copyright (C) 2020, Huada Semiconductor Co., Ltd. All rights reserved.
 *
 * This software component is licensed by HDSC under BSD 3-Clause license
 * (the "License"); You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                    opensource.org/licenses/BSD-3-Clause
 */
/******************************************************************************/
/** \file main.c
 **
 ** \brief USB composite device(HID+CDC) example.
 **
 **   - 2021-04-14  Linsq First version for USB composite device demo.
 **
 ******************************************************************************/

#include "hc32_ddl.h"
#include "usb_dev_user.h"
#include "usb_dev_desc.h"
#include "usb_bsp.h"
#include "usb_dev_hid_cdc_wrapper.h"

usb_core_instance  usb_dev;

void SendKeyValue(uint8_t key)
{
    uint8_t tmp_buf[8]={0xfe,1,0,0,0,0,0,0};
    switch(key)
    {
        case 1:
            tmp_buf[2] = 0x04;     //'a'
            break;
        case 2:
            tmp_buf[2] = 0x05;     //'b'
            break;
        case 3:
            tmp_buf[2] = 0x06;     //'c'
            break;
        case 4:
            tmp_buf[2] = 0x07;     //'d'
            break;
        default:
            break;
    }
    if(key != 0 )
    {
        hd_usb_deveptx(&usb_dev, HID_IN_EP, tmp_buf, 8);
    }
}

int32_t main (void)
{
	uint8_t key_stat_tmp;

	    uint8_t tmp_buf[8]={0xfe,1,0,0,0,0,0,0};
    uint8_t press_status;
    press_status = 0;
		 
    hd_usb_dev_init(&usb_dev, &user_desc, &class_composite_cbk, &user_cb);
    while (1)
    {
		 /*
		 		  Ddl_Delay1ms(8000);
        key_stat_tmp = 1;
        if(key_stat_tmp != 0)
        {
            press_status = 1;
            SendKeyValue(key_stat_tmp);
            Ddl_Delay1ms(20);
        }

        key_stat_tmp = 0;
        if((key_stat_tmp == 0)&&(press_status == 1))
        {
            press_status = 0;
            hd_usb_deveptx(&usb_dev, HID_IN_EP, tmp_buf, 8);
            Ddl_Delay1ms(20);
        }
		 */
    }
}

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/

