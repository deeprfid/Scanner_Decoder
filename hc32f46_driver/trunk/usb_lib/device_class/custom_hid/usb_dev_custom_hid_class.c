/*******************************************************************************
 * Copyright (C) 2020, Huada Semiconductor Co., Ltd. All rights reserved.
 *
 * This software component is licensed by HDSC under BSD 3-Clause license
 * (the "License"); You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                    opensource.org/licenses/BSD-3-Clause
 */
/******************************************************************************/
/** \file usb_dev_custom_hid_class.c
 **
 ** A detailed description is available at
 ** @link
        This file provides the CUSTOM HID core functions.
    @endlink
 **
 **   - 2021-04-14  CDT   First version for USB demo.
 **
 ******************************************************************************/

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "usb_dev_custom_hid_class.h"
#include "usb_dev_desc.h"
#include "usb_dev_stdreq.h"
#include "usb_bsp.h"
#include "hc32_ddl.h"
#include "io_stream.h"
/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/
extern volatile int cushid_send_fin;

__USB_ALIGN_BEGIN uint8_t Report_buf[HID_IN_PACKET] __USB_ALIGN_END ;

//uint8_t Report_buf[HID_IN_PACKET];
uint8_t Send_Buf[HID_OUT_PACKET];
__IO uint32_t IsReportAvailable = 0ul;
/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/
void usb_dev_hid_init(void *pdev);
void usb_dev_hid_deinit(void *pdev);
uint8_t usb_dev_hid_setup(void *pdev, USB_SETUP_REQ *req);
uint8_t *usb_dev_hid_getcfgdesc(uint16_t *length);
void usb_dev_hid_datain(void *pdev, uint8_t epnum);
void usb_dev_hid_dataout(void *pdev, uint8_t epnum);
void usb_dev_hid_ctrlep_rxready(void *pdev);
/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/
usb_dev_class_func  class_customhid_cbk =
{
    &usb_dev_hid_init,
    &usb_dev_hid_deinit,
    &usb_dev_hid_setup,
    NULL, 
    &usb_dev_hid_ctrlep_rxready, 
    &usb_dev_hid_getcfgdesc,
    NULL,
    &usb_dev_hid_datain, 
    &usb_dev_hid_dataout, 
    NULL,
    NULL,
};

__USB_ALIGN_BEGIN static uint32_t  USB_DEV_HID_AltSet  __USB_ALIGN_END = 0u;

__USB_ALIGN_BEGIN static uint32_t  USB_DEV_HID_Protocol  __USB_ALIGN_END = 0u;

__USB_ALIGN_BEGIN static uint32_t  USB_DEV_HID_IdleState __USB_ALIGN_END = 0u;

__USB_ALIGN_BEGIN static uint8_t usb_dev_hid_cfgdesc[USB_CUSTOM_HID_CONFIG_DESC_SIZ] __USB_ALIGN_END =
{
    0x09,                           
    USB_CFG_DESCRIPTOR_TYPE,       
    USB_CUSTOM_HID_CONFIG_DESC_SIZ, 
    0x00,
    0x01,         
    0x01,        
    0x00,         
    0xC0,         
    0x32,         

    0x09,         
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x00,        
    0x00,         
    0x02,       
    0x03,         
    0x00,         
    0x00,         
    0x00,         
   
    0x09,       
    CUSTOM_HID_DESCRIPTOR_TYPE,
    0x10,        
    0x01,
    0x21,         
    0x01,         
    0x22,        
    CUSTOM_HID_REPORT_DESC_SIZE,
    0x00,
    
    0x07,          
    USB_ENDPOINT_DESCRIPTOR_TYPE, 
    HID_IN_EP,     
    0x03,          
    HID_IN_PACKET, 
    0x00,
    0x01,        

    0x07,          
    USB_ENDPOINT_DESCRIPTOR_TYPE, 

    HID_OUT_EP,    
    0x03,          
    HID_OUT_PACKET, 
    0x00,
    0x01,         
} ;


__USB_ALIGN_BEGIN static uint8_t CUSTOM_HID_ReportDesc[CUSTOM_HID_REPORT_DESC_SIZE] __USB_ALIGN_END =
{
    0x05, 0x8C, 
	 0x09, 0x06, 
	 0xa1, 0x01, 
	 0x09, 0x06, 
	 0x15, 0x00,	
	 0x26, 0x00,
    0xff, 0x75, 
	 0x08, 0x95, 
	 0x40, 0x91, 
	 0x82, 0x09, 
	 0x06, 0x15, 
	 0x00, 0x26, 
    0x00, 0xff, 
	 0x75, 0x08, 
	 0x95, 0X40, 
	 0x81, 0x82, 
	 0xc0      
};

/**
 *******************************************************************************
 ** \brief  Initialize the HID application
 ** \param  pdev: Device instance
 ** \retval none
 ******************************************************************************/
void usb_dev_hid_init(void *pdev)
{
    hd_usb_opendevep(pdev, HID_IN_EP, HID_IN_PACKET, USB_EP_INT);
    hd_usb_opendevep(pdev, HID_OUT_EP, HID_OUT_PACKET, USB_EP_INT);
    hd_usb_readytorx(pdev, HID_OUT_EP, Report_buf, HID_IN_PACKET);
    hd_usb_devepstatusset(pdev , HID_OUT_EP , USB_EP_RX_VALID);
}

/**
 *******************************************************************************
 ** \brief  Deinitialize  the HID application
 ** \param  pdev: Device instance
 ** \retval none
 ******************************************************************************/
void usb_dev_hid_deinit(void *pdev)
{
    hd_usb_shutdevep(pdev, HID_IN_EP);
    hd_usb_shutdevep(pdev, HID_OUT_EP);
}

/**
 *******************************************************************************
 ** \brief  Handle the setup requests
 ** \param  pdev: Device instance
 ** \param  req: usb requests
 ** \retval status
 ******************************************************************************/
uint8_t usb_dev_hid_setup(void *pdev, USB_SETUP_REQ *req)
{
    uint8_t hid_report_length = 0u;
    uint16_t len              = 0u;
    uint8_t  *pbuf            = NULL;
    uint8_t u8Ret             = USBD_OK;

    switch (req->bmRequest & USB_REQ_TYPE_MASK)
    {
        case USB_REQ_TYPE_CLASS :
            switch (req->bRequest)
            {
                case CUSTOM_HID_REQ_SET_PROTOCOL:
                    USB_DEV_HID_Protocol = (uint8_t)(req->wValue);
                    break;
                case CUSTOM_HID_REQ_GET_PROTOCOL:
                    hd_usb_ctrldatatx(pdev, (uint8_t *)&USB_DEV_HID_Protocol, 1u);
                    break;
                case CUSTOM_HID_REQ_SET_IDLE:
                    USB_DEV_HID_IdleState = (uint8_t)(req->wValue >> 8);
                    break;
                case CUSTOM_HID_REQ_GET_IDLE:
                    hd_usb_ctrldatatx(pdev, (uint8_t *)&USB_DEV_HID_IdleState, 1u);
                    break;
                case CUSTOM_HID_REQ_SET_REPORT:
                    IsReportAvailable = 1u;
                    hid_report_length = (uint8_t)(req->wLength);
                    hd_usb_ctrldatarx(pdev, Report_buf, (uint16_t)hid_report_length);
                    break;
                default:
                    hd_usb_ctrlerr(pdev);
                    u8Ret = USBD_FAIL;
                    break;
            }
            break;
        case USB_REQ_TYPE_STANDARD:
            switch (req->bRequest)
            {
                case USB_REQ_GET_DESCRIPTOR:
                    if( req->wValue >> 8u == CUSTOM_HID_REPORT_DESC)
                    {
                        len = __MIN(CUSTOM_HID_REPORT_DESC_SIZE, req->wLength);
                        pbuf = (uint8_t*)CUSTOM_HID_ReportDesc;
                    }
                    else if( req->wValue >> 8u == CUSTOM_HID_DESCRIPTOR_TYPE)
                    {
                        pbuf = (uint8_t*)usb_dev_hid_cfgdesc + 0x12u;
                        len = __MIN(USB_CUSTOM_HID_DESC_SIZ, req->wLength);
                    }
                    else
                    {
                        //
                    }

                    hd_usb_ctrldatatx(pdev, pbuf, len);
                    break;
                case USB_REQ_GET_INTERFACE :
                    hd_usb_ctrldatatx(pdev, (uint8_t *)&USB_DEV_HID_AltSet, 1u);
                    break;
                case USB_REQ_SET_INTERFACE :
                    USB_DEV_HID_AltSet = (uint8_t)(req->wValue);
                    break;
            }
            break;
    }
    return u8Ret;
}

/**
 *******************************************************************************
 ** \brief  Send HID Report
 ** \param  pdev: device instance
 ** \param  buff: pointer to report
 ** \param  len: the length in bytes would be sent
 ** \retval status
 ******************************************************************************/
uint8_t usb_dev_hid_txreport(usb_core_instance *pdev, uint8_t *report, uint16_t len)
{
    if (pdev->dev.device_cur_status == USB_DEV_CONFIGURED )
    {
//		 printf("usb_dev_hid_txreport\n");
        hd_usb_deveptx(pdev, HID_IN_EP, report, (uint32_t)len);
    }
    return USBD_OK;
}

/**
 *******************************************************************************
 ** \brief  return configuration descriptor
 ** \param  length : length of configuration descriptor in bytes
 ** \retval pointer to configuration descriptor buffer
 ******************************************************************************/
uint8_t *usb_dev_hid_getcfgdesc(uint16_t *length)
{
    *length = (uint8_t)sizeof(usb_dev_hid_cfgdesc);
    return usb_dev_hid_cfgdesc;
}

/**
 *******************************************************************************
  ** \brief  processing for data in
  ** \param  pdev: device instance
  ** \param  epnum: endpoint index
  ** \retval none
 ******************************************************************************/
void usb_dev_hid_datain(void *pdev, uint8_t epnum)
{
    hd_usb_flsdevep(pdev, HID_IN_EP);

    if (epnum == (HID_IN_EP & (uint8_t)~0x80u))
    {
        cushid_send_fin = 1u;
    }
}
/**
 *******************************************************************************
  ** \brief  processing for data out
  ** \param  pdev: device instance
  ** \param  epnum: endpoint index
  ** \retval none
 ******************************************************************************/
uint8_t cus_hid_rx_buf[USB_COMPO_RXBUF_LEN];
void usb_dev_hid_dataout(void *pdev, uint8_t epnum)
{
//	printf("usb_dev_hid_dataout\n");
    if (epnum == HID_OUT_EP)
    {
		 /*
		 uint16_t usb_rx_cnt;
		 int fbcnt;
		 int sbcnt;

		 usb_rx_cnt = (uint16_t)((usb_core_instance*)pdev)->dev.out_ep[epnum].xfer_count;
		 printf("usb_dev_hid_dataout usb_rx_cnt:%d\n", usb_rx_cnt);
		 if (cus_hid_rxbuf_windex+usb_rx_cnt <= CUS_HID_RXBUF_LEN)
		 {
			 memcpy(cus_hid_rx_buf+cus_hid_rxbuf_windex, Report_buf, usb_rx_cnt);
			 cus_hid_rxbuf_windex += usb_rx_cnt;			 
		 }
		 else
		 {
			 fbcnt = CUS_HID_RXBUF_LEN - cus_hid_rxbuf_windex;
			 sbcnt = usb_rx_cnt - fbcnt;
			 memcpy(cus_hid_rx_buf+cus_hid_rxbuf_windex, Report_buf, fbcnt);
			 memcpy(cus_hid_rx_buf, Report_buf+fbcnt, sbcnt);
			 cus_hid_rxbuf_windex = sbcnt;
		 }
		 */
		 uint16_t usb_rx_cnt;
		 int fbcnt;
		 int sbcnt;
		 int reallen;
		 int i;
		 uint16 crc = 0;
		 uint16 rpcrc;
		 usb_rx_cnt = (uint16_t)((usb_core_instance*)pdev)->dev.out_ep[epnum].xfer_count;
		 
		 if (usb_rx_cnt > 1)
		 {
			 reallen = Report_buf[0];
			 if (reallen < 62)
			 {
//				 printf("hid_dataout:");
				 for (i = 0; i < reallen; ++i)
				 {
//					printf("%02X ", Report_buf[1+i]);
					crc += Report_buf[1+i];
				 }
//				 printf("\n");
				 rpcrc = (Report_buf[1+reallen] << 8) | Report_buf[2+reallen];
				 if (crc == rpcrc)
				 {	
//					 printf("rx_cnt:%d, realcnt:%d\n", usb_rx_cnt, reallen);
					 if (gUsbParams[0].usb_tail+reallen <= USB_COMPO_RXBUF_LEN)
					 {
						 memcpy_byb(cus_hid_rx_buf+gUsbParams[0].usb_tail, Report_buf+1, reallen);
						 if (gUsbParams[0].usb_tail + reallen >= USB_COMPO_RXBUF_LEN)
							 gUsbParams[0].usb_tail = 0;
						 else
							 gUsbParams[0].usb_tail += reallen;
					 }
					 else
					 {
						 fbcnt = USB_COMPO_RXBUF_LEN - gUsbParams[0].usb_tail;
						 sbcnt = reallen - fbcnt;
						 memcpy_byb(cus_hid_rx_buf+gUsbParams[0].usb_tail, Report_buf+1, fbcnt);
						 memcpy_byb(cus_hid_rx_buf, Report_buf+fbcnt+1, sbcnt);
						 gUsbParams[0].usb_tail = sbcnt;
					 }
				 }
				 else
					 TRACE("crc error errorerror\n");
			 }
			 else
				 TRACE("Report_buf[0] error\n");
		 }
	 }

    hd_usb_readytorx(pdev,HID_OUT_EP,Report_buf, HID_IN_PACKET);
    hd_usb_devepstatusset(pdev , HID_OUT_EP , USB_EP_RX_VALID);
}

/**
 *******************************************************************************
 ** \brief  processing for request data in control endpoint
 ** \param  pdev: device instance
 ** \retval none
 ******************************************************************************/
void usb_dev_hid_ctrlep_rxready(void *pdev)
{
    if (IsReportAvailable == 1u)
    {
        IsReportAvailable = 0u;
    }
}

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
