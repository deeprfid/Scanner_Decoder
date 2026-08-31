/*******************************************************************************
 * Copyright (C) 2020, Huada Semiconductor Co., Ltd. All rights reserved.
 *
 * This software component is licensed by HDSC under BSD 3-Clause license
 * (the "License"); You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                    opensource.org/licenses/BSD-3-Clause
 */
/******************************************************************************/
/** \file usb_dev_cdc_class.c
 **
 ** A detailed description is available at
 ** @link
      This file provides the CDC VCP core functions.
  @endlink
 ** @brief   This file provides the high layer firmware functions to manage the
 **          following functionalities of the USB CDC Class:
 **           - Initialization and Configuration of high and low layer
 **           - Enumeration as CDC Device (and enumeration for each implemented memory interface)
 **           - OUT/IN data transfer
 **           - Command IN transfer (class requests management)
 **           - Error management
 **
 **   - 2021-04-16  CDT   First version for USB CDC VCP demo.
 **
 ******************************************************************************/

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "usb_dev_cdc_class.h"
#include "usb_dev_desc.h"
#include "usb_dev_stdreq.h"
#include "usb_app_conf.h"
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

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/
/*********************************************
   CDC Device library callbacks
 *********************************************/
void usb_dev_cdc_init(void *pdev);
void usb_dev_cdc_deinit(void *pdev);
uint8_t usb_dev_cdc_setup(void *pdev, USB_SETUP_REQ *req);
void usb_dev_cdc_ctrlep_rxready(void *pdev);
void usb_dev_cdc_datain(void *pdev, uint8_t epnum);
void usb_dev_cdc_dataout(void *pdev, uint8_t epnum);
uint8_t usb_dev_cdc_sof(void *pdev);
uint8_t *usb_dev_cdc_getcfgdesc(uint16_t *length);

/*********************************************
   CDC specific management functions
 *********************************************/
void process_asynchdata_uart2usb(void *pdev);

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/
__USB_ALIGN_BEGIN static uint32_t  alternate_setting  __USB_ALIGN_END = 0ul;
__USB_ALIGN_BEGIN uint8_t usb_rx_buffer[MAX_CDC_PACKET_SIZE] __USB_ALIGN_END ;
//uint8_t uart_rx_buffer[APP_RX_DATA_SIZE];  //used as a buffer for receiving data from uart port
__USB_ALIGN_BEGIN uint8_t CmdBuff[CDC_CMD_PACKET_SZE] __USB_ALIGN_END ;
uint32_t APP_Rx_ptr_in  = 0ul;
uint32_t APP_Rx_ptr_out = 0ul;
uint32_t APP_Rx_length  = 0ul;
uint8_t  USB_Tx_State   = 0u;
static uint32_t cdcCmd  = 0xFFul;
//static uint32_t cdcLen  = 0ul;
//static uint32_t LastPackLen = 0;

usb_dev_class_func  class_cdc_cbk =
{
    &usb_dev_cdc_init,
    &usb_dev_cdc_deinit,
    &usb_dev_cdc_setup,
    NULL,
    &usb_dev_cdc_ctrlep_rxready,
    &usb_dev_cdc_getcfgdesc,
    &usb_dev_cdc_sof,
    &usb_dev_cdc_datain,
    &usb_dev_cdc_dataout,
    NULL,
    NULL,
};

__USB_ALIGN_BEGIN uint8_t usb_dev_cdc_cfgdesc[USB_CDC_CONFIG_DESC_SIZ]  __USB_ALIGN_END =
{
    0x09,
    USB_CFG_DESCRIPTOR_TYPE,
    USB_CDC_CONFIG_DESC_SIZ,
    0x00,
    0x02,
    0x01,
    0x00,
    0xC0,
    0x32,

    0x08,
    0x0b,
    0x00,
    0x02,
    0x02,
    0x02,
    0x01,
    0x04,

    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x00,
    0x00,
    0x01,
    0x02,
    0x02,
    0x01,
    0x00,

    0x05,
    0x24,
    0x00,
    0x10,
    0x01,

    0x05,
    0x24,
    0x01,
    0x00,
    0x01,

    0x04,
    0x24,
    0x02,
    0x02,

    0x05,
    0x24,
    0x06,
    0x00,
    0x01,

    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    CDC_CMD_EP,
    0x03,
    LOBYTE(CDC_CMD_PACKET_SZE),
    HIBYTE(CDC_CMD_PACKET_SZE),
    0xFF,

    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x01,
    0x00,
    0x02,
    0x0A,
    0x00,
    0x00,
    0x00,

    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    CDC_OUT_EP,
    0x02,
    LOBYTE(MAX_CDC_PACKET_SIZE),
    HIBYTE(MAX_CDC_PACKET_SIZE),
    0x00,

    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    CDC_IN_EP,
    0x02,
    LOBYTE(MAX_CDC_PACKET_SIZE),
    HIBYTE(MAX_CDC_PACKET_SIZE),
    0x00
} ;

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 *******************************************************************************
 ** \brief  Initilaize the CDC application
 ** \param  pdev: Device instance
 ** \retval none
 ******************************************************************************/
void usb_dev_cdc_init(void *pdev)
{
    hd_usb_opendevep(pdev, CDC_IN_EP, MAX_CDC_IN_PACKET_SIZE, USB_EP_BULK);
    hd_usb_opendevep(pdev, CDC_OUT_EP, MAX_CDC_OUT_PACKET_SIZE, USB_EP_BULK);
    hd_usb_opendevep(pdev, CDC_CMD_EP, CDC_CMD_PACKET_SZE, USB_EP_INT);
//    vcp_init();
    hd_usb_readytorx(pdev, CDC_OUT_EP, (uint8_t*)(usb_rx_buffer), MAX_CDC_OUT_PACKET_SIZE);
}

/**
 *******************************************************************************
 ** \brief  Deinitialize the CDC application
 ** \param  pdev: Device instance
 ** \retval none
 ******************************************************************************/
void usb_dev_cdc_deinit(void *pdev)
{
    hd_usb_shutdevep(pdev, CDC_IN_EP);
    hd_usb_shutdevep(pdev, CDC_OUT_EP);
    hd_usb_shutdevep(pdev, CDC_CMD_EP);
//    vcp_deinit();
}

/**
 *******************************************************************************
 ** \brief  Handle the setup requests
 ** \param  pdev: Device instance
 ** \param  req: usb requests
 ** \retval status
 ******************************************************************************/
uint8_t usb_dev_cdc_setup(void *pdev, USB_SETUP_REQ *req)
{
    uint16_t len=USB_CDC_DESC_SIZ;
    uint8_t  *pbuf=usb_dev_cdc_cfgdesc + 9;
    uint8_t u8Res = USBD_OK;

    switch (req->bmRequest & USB_REQ_TYPE_MASK)
    {
        case USB_REQ_TYPE_CLASS :
            if (req->wLength)
            {
                if (req->bmRequest & 0x80u)
                {
//                    vcp_ctrlpare(req->bRequest, CmdBuff, req->wLength);
                    hd_usb_ctrldatatx(pdev, CmdBuff, req->wLength);
                }
                else
                {
                    cdcCmd = req->bRequest;
//                    cdcLen = req->wLength;
                    hd_usb_ctrldatarx(pdev, CmdBuff, req->wLength);
                }
            }
            else
            {
//                vcp_ctrlpare(req->bRequest, NULL, 0);
            }
            break;
        case USB_REQ_TYPE_STANDARD:
            switch (req->bRequest)
            {
                case USB_REQ_GET_DESCRIPTOR:
                    if((req->wValue>>8) == CDC_DESCRIPTOR_TYPE)
                    {
                        pbuf = usb_dev_cdc_cfgdesc + 9u + (9u * USBD_ITF_MAX_NUM);
                        len  = __MIN(USB_CDC_DESC_SIZ , req->wLength);
                    }
                    hd_usb_ctrldatatx(pdev, pbuf, len);
                    break;

                case USB_REQ_GET_INTERFACE :
                    hd_usb_ctrldatatx(pdev, (uint8_t *)&alternate_setting, 1u);
                    break;

                case USB_REQ_SET_INTERFACE :
                    if ((uint8_t)(req->wValue) < USBD_ITF_MAX_NUM)
                    {
                        alternate_setting = (uint8_t)(req->wValue);
                    }
                    else
                    {
                        hd_usb_ctrlerr(pdev);
                    }
                    break;
                default:
                    break;
            }
            break;

        default:
            hd_usb_ctrlerr (pdev);
            u8Res = USBD_FAIL;
            break;
    }
    return u8Res;
}

/**
 *******************************************************************************
 ** \brief  Data received on control endpoint
 ** \param  pdev: device device instance
 ** \retval none
 ******************************************************************************/
void usb_dev_cdc_ctrlep_rxready(void *pdev)
{
    if (cdcCmd != NO_CMD)
    {
//        vcp_ctrlpare(cdcCmd, CmdBuff, cdcLen);
        cdcCmd = NO_CMD;
    }
}

/**
 *******************************************************************************
 ** \brief  Data sent on non-control IN endpoint
 ** \param  pdev: Device instance
 ** \param  epnum: endpoint index
 ** \retval none
 ******************************************************************************/
extern volatile int kbcdc_send_fin;
void usb_dev_cdc_datain(void *pdev, uint8_t epnum)
{
    hd_usb_flsdevep(pdev, CDC_IN_EP);
    if (epnum == (CDC_IN_EP & (uint8_t)~0x80u))
    {
        kbcdc_send_fin = 1u;
    }
}

/**
 *******************************************************************************
 ** \brief  Data received on non-control Out endpoint
 ** \param  pdev: device instance
 ** \param  epnum: endpoint index
 ** \retval none
 ******************************************************************************/
//#define CDC_RXBUF_LEN 768
uint8_t cdc_rx_buf[USB_COMPO_RXBUF_LEN];
void usb_dev_cdc_dataout(void *pdev, uint8_t epnum)
{
   uint16_t usb_rx_cnt;
	int fbcnt;
	int sbcnt;
   usb_rx_cnt = (uint16_t)((usb_core_instance*)pdev)->dev.out_ep[epnum].xfer_count;
//	printf("usb_dev_hid_dataout usb_rx_cnt:%d\n", usb_rx_cnt);
	if (gUsbParams[1].usb_tail+usb_rx_cnt <= USB_COMPO_RXBUF_LEN)
	{
		memcpy_byb(cdc_rx_buf+gUsbParams[1].usb_tail, usb_rx_buffer, usb_rx_cnt);
		if (gUsbParams[1].usb_tail + usb_rx_cnt >= USB_COMPO_RXBUF_LEN)
			gUsbParams[1].usb_tail = 0;
		else
			gUsbParams[1].usb_tail += usb_rx_cnt;
	}
	else
	{
		fbcnt = USB_COMPO_RXBUF_LEN - gUsbParams[1].usb_tail;
		sbcnt = usb_rx_cnt - fbcnt;
		memcpy_byb(cdc_rx_buf+gUsbParams[1].usb_tail, usb_rx_buffer, fbcnt);
		memcpy_byb(cdc_rx_buf, usb_rx_buffer+fbcnt, sbcnt);
		gUsbParams[1].usb_tail = sbcnt;
	}
		 
//    vcp_rxdata(usb_rx_buffer, usb_rx_cnt);
    hd_usb_readytorx(pdev, CDC_OUT_EP, (uint8_t*)(usb_rx_buffer), MAX_CDC_OUT_PACKET_SIZE);
}

/**
 *******************************************************************************
 ** \brief  Start Of Frame event management
 ** \param  pdev: Device instance
 ** \retval status
 ******************************************************************************/
uint8_t usb_dev_cdc_sof(void *pdev)
{
	/*
    static uint32_t FrameCount = 0ul;

    if (FrameCount++ == CDC_IN_FRAME_INTERVAL)
    {
        FrameCount = 0ul;
        process_asynchdata_uart2usb(pdev);
    }
	*/
    return USBD_OK;
}

/**
 *******************************************************************************
 ** \brief  get the configuration descriptor
 ** \param  length : length of configuration descriptor in bytes
 ** \retval the pointer to configuration descriptor buffer
 ******************************************************************************/
uint8_t *usb_dev_cdc_getcfgdesc(uint16_t *length)
{
    *length = (uint16_t)sizeof (usb_dev_cdc_cfgdesc);
    return usb_dev_cdc_cfgdesc;
}

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
