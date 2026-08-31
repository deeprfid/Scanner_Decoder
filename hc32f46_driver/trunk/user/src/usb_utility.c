#include "usb_dev_user.h"
#include "usb_dev_desc.h"
#include "cdc_data_process.h"
#include "usb_bsp.h"
#include "usb_dev_hid_cdc_wrapper.h"
#include "usb_dev_custom_hid_class.h"
#include "usb_dev_keyboard_class.h"
#include "hc32f46_driver.h"
#include "io_stream.h"

usb_core_instance  usb_dev;

extern volatile int gIsUsbAvailable;
volatile int cus_hid_rxbuf_rindex = 0;

int IsUsbAvailable(void)
{
	return gIsUsbAvailable;
}
int init_usb(int type)
{
	gUsbParams[0].isBlock = O_BLOCK;
	gUsbParams[0].timeout = -1;
	gUsbParams[0].recvbuf = cus_hid_rx_buf;
	gUsbParams[0].recvbufsize = USB_COMPO_RXBUF_LEN;
	gUsbParams[0].usb_head = 0;
	gUsbParams[0].usb_tail = 0;
	
	gUsbParams[1].isBlock = O_BLOCK;
	gUsbParams[1].timeout = -1;
	gUsbParams[1].recvbuf = cdc_rx_buf;
	gUsbParams[1].recvbufsize = USB_COMPO_RXBUF_LEN;
	gUsbParams[1].usb_head = 0;
	gUsbParams[1].usb_tail = 0;
	
	gUsbCompType = type;
	hd_usb_dev_init(&usb_dev, &user_desc, &class_composite_cbk, &user_cb);
	return 0;
}

void send_key(uint8_t key, int isupper)
{
	uint8_t report[8]={0,0,0,0,0,0,0,0};
	if (gIsUsbAvailable == 0)
		return;
	report[2] = key;
	if (key >= 0x04 && key <= 0x09 && isupper == 1)
		report[0] = 0x02;

	usb_dev_kbd_txreport(&usb_dev, report, 8);
	memset(report, 0, 8);
	usb_dev_kbd_txreport(&usb_dev, report, 8);
}
/*
void deinitUSB()
{
	hd_usb_gintdis(&usb_dev);
	CLK_UpllCmd(Disable);
}
*/
/*
int usb_send(const void *buf, uint32_t len)
{
	int i;
	int scnt = len / 64;
	int lastcnt = len % 64;
	if (gIsUsbAvailable == 0)
		return -1;
	for (i = 0; i < scnt; ++i)
		usb_dev_hid_txreport(&usb_dev, (uint8_t *)buf+i*64, 64);
	if (lastcnt != 0)
		usb_dev_hid_txreport(&usb_dev, (uint8_t *)buf+i*64, lastcnt);
	return len;
}
*/

int usb_send(int uid, const void *buf, uint32_t len)
{
	int i;
	int j;
	int scnt;
	int lastcnt;
	uint16_t crc;
	
	if (gIsUsbAvailable == 0)
		return -1;
//	printf("usb_send start ---------------------------\n");
	if (uid == 0)
	{
		uint8_t report[64];
		scnt = len / 61;
		lastcnt = len % 61;
		crc = 0;
		
		for (i = 0; i < scnt; ++i)
		{
			crc = 0;
			report[0] = 61;
			memcpy(report+1, (uint8_t *)buf+i*61, 61);
			for (j = 0; j < 61; ++j)
				crc += ((uint8_t *)buf+i*61)[j];
			report[62] = (crc >> 8) & 0xff;
			report[63] = (crc >> 0) & 0xff;
			usb_dev_hid_txreport(&usb_dev, report, 64);
			/*
			printf("usb send:");
			for (j = 0; j < 64;++j)
				printf("%02X ", report[j]);
			printf("\n");
			*/
		}
		if (lastcnt != 0)
		{
			crc = 0;
			report[0] = lastcnt;
			memcpy(report+1, (uint8_t *)buf+i*61, lastcnt);
			for (j = 0; j < lastcnt; ++j)
				crc += ((uint8_t *)buf+i*61)[j];
			report[lastcnt+1] = (crc >> 8) & 0xff;
			report[lastcnt+2] = (crc >> 0) & 0xff;
			usb_dev_hid_txreport(&usb_dev, report, 64);
			/*
			printf("usb send:");
			for (j = 0; j < lastcnt+3;++j)
				printf("%02X ", report[j]);
			printf("\n");
			*/
		}
	}
	else if (uid == 1)
	{
		scnt = len / 62;
		lastcnt = len % 62;
		for (i = 0; i < scnt; ++i)
			hd_usb_deveptx(&usb_dev, CDC_IN_EP, (uint8 *)buf+i*62, 62);
		
		if (lastcnt != 0)
			hd_usb_deveptx(&usb_dev, CDC_IN_EP, (uint8 *)buf+i*62, lastcnt);
		
//		hd_usb_deveptx(&usb_dev, CDC_IN_EP, (uint8 *)buf, len);
	}
	else
		return -1;
//	printf("usb_send end ---------------------------\n");
	return len;
}


int usb_recv(int uid, void *buf, uint32_t len)
{
	int recvLen=0;
	uint16 usb_tail_now;
	if(gIsUsbAvailable == 0)
		return 0;
	commonUsbParaLocal *ubpara = &gUsbParams[uid];
	
	usb_tail_now = ubpara->usb_tail;
	if (usb_tail_now == ubpara->usb_head)
		return 0;
	else
	{
//		printf("1111 windex:%d, rindex:%d, len:%d\n", ubpara->usb_tail, 
//			ubpara->usb_head, len);
		if (usb_tail_now > ubpara->usb_head)
		{
			recvLen = usb_tail_now - ubpara->usb_head;
			if(recvLen > len)
				recvLen = len;
			memcpy_byb(buf, ubpara->recvbuf+ubpara->usb_head, recvLen);
			ubpara->usb_head += recvLen;
//			printf("00000  recvLen:%d\n", recvLen);
		}
		else
		{
			recvLen = ubpara->recvbufsize - ubpara->usb_head;
			if(recvLen > len)
				recvLen = len;

			memcpy_byb(buf, ubpara->recvbuf+ubpara->usb_head, recvLen);
			ubpara->usb_head += recvLen;
//			printf("11111  recvLen:%d\n", recvLen);
			if (recvLen < len)
			{
				int band2len = len - recvLen;
				if (usb_tail_now <= band2len)
					band2len = usb_tail_now;
				memcpy_byb((char *)buf+recvLen, ubpara->recvbuf, band2len);
				recvLen += band2len;
				ubpara->usb_head = band2len;
//				printf("11111  band2len:%d\n", band2len);
			}
		}
	}
	
//	printf("2222 windex:%d, rindex:%d, len:%d\n", ubpara->usb_tail, 
//		ubpara->usb_head, len);
	/*
	for (i = 0; i < recvLen; ++i)
		printf("%02X ", ((char*)buf)[i]);
	printf("\n");
	*/
	if (ubpara->usb_head >= ubpara->recvbufsize)
		ubpara->usb_head = 0;
	return recvLen;
}

