#include <stdlib.h>
#include <string.h>
#include "hc32f46_driver.h"
#include "ModuleReader.h"
#include "mp_pool.h"
#include "customcmd.h"
#include "reader_init.h"

extern ReaderStaticSettings_ST *gPRdrStaSet;
extern int gRdrHandlePassive;
extern int gAntNumber;

READER_ERR getModlueParams(int hreader, ReaderStaticSettings_ST *pRsSettings);

#define SET_DETAIL_ERR(apierr) do \
	{ \
		int dterr; \
		char *dtstr; \
		if (apierr == MT_OK_ERR) \
			SetNumU16(g_regs_rdrsta->val, 0); \
		else \
		{ \
			TRACE("!!!!!!!!!!!! api err:%d\n", apierr); \
			GetLastDetailError(gRdrHandlePassive, &dterr, &dtstr); \
			SetNumU16(g_regs_rdrsta->val, dterr); \
		} \
	} while(0)
	

typedef struct mb_regs_st
{
	uint16 addr_bs;
	uint8 *val;
	uint16 mwdcnt;
	uint8 isasepart;
	uint8 tagbank;
//	uint8 canbak;
	int (* cmd_03_handler)(struct mb_regs_st *, uint8 *, uint8 *, uint8 *);
	int (* cmd_06_handler)(struct mb_regs_st *, uint8 *, uint8 *, uint8 *);
	int (* cmd_16_handler)(struct mb_regs_st *, uint8 *, uint8 *, uint8 *);
	int (* valid_regval)(struct mb_regs_st *, uint16 startaddr, 
		uint8 *para, uint16 regcnt);
	struct mb_regs_st *next;
} mb_regs_st;

mb_regs_st *g_reg_head;
mb_regs_st *g_regs_rdrsta;
mb_regs_st *g_regs_tagbytes;
uint8 *g_mb_send_buf;
uint8 *g_mb_tcp_sendbuf;
uint8 *g_mb_regvals_bak;
uint16 gMaxTxPower;
uint16 gMinTxPower;
uint16 gModtcpSeriNum;
uint8 gModBusAddr;

void mb_copy_uart_conf(void);
int gMbLastUartCommFd = -1;
rdr_st_set_uart *gPMbCurUartSet;

int is_modbus_potl(int fd, uint8 *buf, int nparse)
{
	if (fd == COMMON_INTERFACE_UART2 || fd == COMMON_INTERFACE_UART3)
	{
		if (fd == COMMON_INTERFACE_UART2)
			gPMbCurUartSet = &gPRdrStaSet->uart1;
		else
			gPMbCurUartSet = &gPRdrStaSet->uart2;
		if (buf[0] == gPMbCurUartSet->address || buf[0] == 0x00)							
		{
			if (buf[1] == 0x03 || buf[1] == 0x06 || buf[1] == 0x10 || 
				buf[1] == 0x01 || buf[1] == 0x02 || buf[1] == 0x04 || 
				buf[1] == 0x05 || buf[1] == 0x0f)
			{
				if (fd != gMbLastUartCommFd)
				{
					mb_copy_uart_conf();
					gMbLastUartCommFd = fd;
				}
				return 1;
			}
		}
	}
	else if (fd == COMMON_INTERFACE_UART1 || fd <= COMMON_INTERFACE_SOCKET1)
	{
		if (nparse == 3)
		{
			if (read(fd, buf+3, 3) != 3)
				return 0;
		}
		if (buf[2] == 0x00 && buf[3] == 0x00 && GetNumU16(buf+4) >= 6)
			return 1;		
	}
	
	return 0;
}

void mb_add_crc(uint8 *data, int len)
{
	uint16 crc;
	crc = crc_Msg(data, len);
	data[len] = (crc >> 8) & 0xff;
	data[len+1] = (crc >> 0) & 0xff;
}

int mb_common_03_resp(mb_regs_st * regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	int pos = 0;
	uint16 rregcnt = GetNumU16(rbuf+4);
	uint16 rstreg = GetNumU16(rbuf+2);
	
	
	sbuf[pos++] = gModBusAddr;
	sbuf[pos++] = rbuf[1];
	sbuf[pos++] = rregcnt*2;
	memcpy(sbuf+pos, regsst->val+(rstreg-regsst->addr_bs)*2, rregcnt*2);
	pos += rregcnt*2;
	mb_add_crc(sbuf, pos);
	*nlen = pos+2;
	return 0;
}

int mb_common_06_resp(mb_regs_st * regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	memcpy(sbuf, rbuf, 8);
	*nlen = 8;
	return 0;
}

int mb_common_16_resp(mb_regs_st * regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	int pos = 6;
	memcpy(sbuf, rbuf, 6);
	mb_add_crc(sbuf, pos);
	*nlen = pos+2;
	return 0;
}

mb_regs_st *find_regs(uint16 addr, int regcnt)
{
	mb_regs_st *regsst = g_reg_head;
	if (regcnt < 1)
		return NULL;
	while (regsst != NULL)
	{
		if (regsst->isasepart == 0)
		{
			if (regsst->addr_bs == addr && regcnt == regsst->mwdcnt)
			{
				TRACE("isasepart = 0 regsst->addr_bs:%d, regsst->mwdcnt:%d\n", 
					regsst->addr_bs, regsst->mwdcnt);
				break;
			}
		}
		else
		{
			if (regsst->addr_bs <= addr && 
				(addr + regcnt) <= (regsst->addr_bs+regsst->mwdcnt))
			{
				TRACE("isasepart = 1 regsst->addr_bs:%d, regsst->mwdcnt:%d\n", 
					regsst->addr_bs, regsst->mwdcnt);
				break;
			}				
		}
		regsst = regsst->next;
	}
	return regsst;
}

int mb_common_err_resp(uint8 cmd, uint8 ecode, uint8 *buf)
{
	int pos = 0;
	buf[pos++] = gModBusAddr;
	buf[pos++] = cmd + 0x80;
	buf[pos++] = ecode;
	mb_add_crc(buf, pos);
	return pos+2;
}

int reg_dev_addr_validval(mb_regs_st *regsst, uint8 *para, int isset)
{
	uint16 val = GetNumU16(para);
	TRACE("reg_dev_addr_validval\n");
	if (val < 1 || val > 255)
		return 3;
	if (isset == 0)
		return 0;
	
	gPMbCurUartSet->address = val;
	SetNumU16(regsst->val, val);
	TRACE("set dev_addr:%d\n", val);
	return 0;
}

uint16 uartconf2regval(rdr_st_set_uart *uart)
{
	uint16 val = 0;
	val |= uart->type;
	switch(uart->baud)
	{
		case 19200:
			val |= (1 << 1);
			break;
		case 38400:
			val |= (2 << 1);
			break;
		case 57600:
			val |= (3 << 1);
			break;
		case 115200:
			val |= (4 << 1);
			break;
		case 230400:
			val |= (5 << 1);
			break;
		case 460800:
			val |= (6 << 1);
			break;
		case 921600:
			val |= (7 << 1);
			break;
	}
	val |= uart->data_bits << 4;
	val |= uart->stop_bits << 5;
	val |= uart->parity << 6;
	val |= uart->flow_ctrl << 8;
	return val;
}
int reg_uart_conf_validval(mb_regs_st *regsst, uint8 *rpara, int isset)
{
	int tmp;
	uint16 val = GetNumU16(rpara);
	TRACE("reg_uart_conf_validval\n");
	if (((val >> 6) & 0x03) == 0x03)
		return 3;
	if (isset == 0)
		return 0;
	SetNumU16(regsst->val+2, val);
	gPMbCurUartSet->type = val & 0x01;
	tmp = (val >> 1) & 0x07;
	switch(tmp)
	{
		case 0:
			gPMbCurUartSet->baud = 9600;
			break;
		case 1:
			gPMbCurUartSet->baud = 19200;
			break;
		case 2:
			gPMbCurUartSet->baud = 38400;
			break;
		case 3:
			gPMbCurUartSet->baud = 57600;
			break;
		case 4:
			gPMbCurUartSet->baud = 115200;
			break;
		case 5:
			gPMbCurUartSet->baud = 230400;
			break;
		case 6:
			gPMbCurUartSet->baud = 460800;
			break;
		case 7:
			gPMbCurUartSet->baud = 921600;
			break;			
	}
	gPMbCurUartSet->data_bits = (val >> 4) & 0x01;
	gPMbCurUartSet->stop_bits = (val >> 5) & 0x01;
	gPMbCurUartSet->parity = (val >> 6) & 0x03;
	TRACE("set uart_conf type:%d,baud:%d,data_bits:%d,stop_bits:%d,parity:%d\n", 
		gPMbCurUartSet->type, gPMbCurUartSet->baud, gPMbCurUartSet->data_bits, 
		gPMbCurUartSet->stop_bits, gPMbCurUartSet->parity);
	
	return 0;
}

int reg_inv_timeout_validval(mb_regs_st *regsst, uint8 *rpara, int isset)
{
	int val = GetNumU16(rpara);
	if (isset == 0)
		return 0;
	SetNumU16(regsst->val+4, val);
	gPRdrStaSet->tagops_param.inventory.cycle = val;
	TRACE("set cycle:%d\n", gPRdrStaSet->tagops_param.inventory.cycle);
	return 0;
}

int reg_inv_ants_validval(mb_regs_st *regsst, uint8 *rpara, int isset)
{
	int i;
	int antcnt = 0;
	uint16 val = GetNumU16(rpara);
	
	for (i = 0; i < 16; ++i)
	{
		if (((val >> i) & 0x01) == 0x01)
		{
			if (i+1 > gAntNumber)
			{
				TRACE("reg_inv_ants_validval failed\n");
				return 3;
			}
		}
	}
	
	if (isset == 0)
		return 0;
	
	SetNumU16(regsst->val+6, val);
	for (i = 0; i < 16; ++i)
	{
		if (((val >> i) & 0x01) == 0x01)
			gPRdrStaSet->tagops_param.inventory.ants[antcnt++] = i+1;
	}
	gPRdrStaSet->tagops_param.inventory.ants_cnt = antcnt;
	//
	TRACE("set ants:");
	for (i = 0; i < gPRdrStaSet->tagops_param.inventory.ants_cnt; ++i)
		TRACE("%d ", gPRdrStaSet->tagops_param.inventory.ants[i]);
	TRACE("\n");
	//
	return 0;
}

int reg_tagop_ant_validval(mb_regs_st *regsst, uint8 *rpara, int isset)
{
	uint16 val = GetNumU16(rpara);
	if (val > gAntNumber)
	{
		TRACE("reg_tagop_ant_validval failed\n");
		return 3;
	}
	if (isset == 0)
		return 0;
	SetNumU16(regsst->val+8, val);
	gPRdrStaSet->tagops_param.accessop.ant = val;
	TRACE("set tagop_ant:%d\n", val);
	return 0;
}

int reg_tagop_timeout_validval(mb_regs_st *regsst, uint8 *rpara, int isset)
{
	uint16 val = GetNumU16(rpara);
	if (isset == 0)
		return 0;
	SetNumU16(regsst->val+10, val);
	gPRdrStaSet->tagops_param.accessop.timeout = val;
	TRACE("set tagop_timeout:%d\n", val);
	return 0;
}

int reg_access_pwd_hb_validval(mb_regs_st *regsst, uint8 *rpara, int isset)
{
	uint16 val = GetNumU16(rpara);
	if (isset == 0)
		return 0;
	SetNumU16(regsst->val+12, val);
	memcpy(gPRdrStaSet->tagops_param.accessop.aespwd, rpara, 2);
	TRACE("set access_pwd hword:%02X%02X", rpara[0], 
		rpara[1]);
	return 0;
}

int reg_access_pwd_lb_validval(mb_regs_st *regsst, uint8 *rpara, int isset)
{
	uint16 val = GetNumU16(rpara);
	if (isset == 0)
		return 0;
	SetNumU16(regsst->val+14, val);
	memcpy(gPRdrStaSet->tagops_param.accessop.aespwd+2, rpara, 2);
	TRACE("set access_pwd lword:%02X%02X", rpara[0], 
		rpara[1]);
	return 0;
}

int reg_conf0000_validval(mb_regs_st *regsst, 
	uint16 startaddr, uint8 *para, uint16 regcnt)
{
	int i;
	int ret;
	uint8 *pa_;
	uint16 regaddr = startaddr;
	
	for (i = 0; i < regcnt; ++i)
	{
		pa_ = para+i*2;
		switch (regaddr)
		{
			case 0x0000:
				ret = reg_dev_addr_validval(regsst, pa_, 0);
				break;
			case 0x0001:
				ret = reg_uart_conf_validval(regsst, pa_, 0);
				break;
			case 0x0002:
				ret = reg_inv_timeout_validval(regsst, pa_, 0);
				break;
			case 0x0003:
				ret = reg_inv_ants_validval(regsst, pa_, 0);
				break;
			case 0x0004:
				ret = reg_tagop_ant_validval(regsst, pa_, 0);
				break;
			case 0x0005:
				ret = reg_tagop_timeout_validval(regsst, pa_, 0);
				break;
			case 0x0006:
				ret = reg_access_pwd_hb_validval(regsst, pa_, 0);
				break;
			case 0x0007:
				ret = reg_access_pwd_lb_validval(regsst, pa_, 0);
				break;
		}
		regaddr++;
		if (ret != 0)
			return ret;
	}
	
	regaddr = startaddr;
	for (i = 0; i < regcnt; ++i)
	{
		pa_ = para+i*2;
		switch (regaddr)
		{
			case 0x0000:
				reg_dev_addr_validval(regsst, pa_, 1);
				break;
			case 0x0001:
				reg_uart_conf_validval(regsst, pa_, 1);
				break;
			case 0x0002:
				reg_inv_timeout_validval(regsst, pa_, 1);
				break;
			case 0x0003:
				reg_inv_ants_validval(regsst, pa_, 1);
				break;
			case 0x0004:
				reg_tagop_ant_validval(regsst, pa_, 1);
				break;
			case 0x0005:
				reg_tagop_timeout_validval(regsst, pa_, 1);
				break;
			case 0x0006:
				reg_access_pwd_hb_validval(regsst, pa_, 1);
				break;
			case 0x0007:
				reg_access_pwd_lb_validval(regsst, pa_, 1);
				break;
		}
		regaddr++;
		if (ret != 0)
			return ret;
	}
	
	return 0;
}

int reg_conf0070_validval(mb_regs_st *regsst, 
	uint16 startaddr, uint8 *para, uint16 regcnt)
{
	int i;
	uint16 tmpval;
	
	for (i = 0; i < regcnt; ++i)
	{
		tmpval = GetNumU16(para+i*2);
		switch (startaddr+i)
		{
			case 0x0070:
			{
				if (tmpval > 3)
					return 3;
				break;
			}
			case 0x0071:
			{
				
				if (tmpval != 0xffff)
				{
					if (tmpval > 15)
						return 3;
				}
				break;
			}
			case 0x0072:
			{
				if (tmpval > 3)
					return 3;
				break;
			}
			case 0x0073:
			{
				if (!(tmpval < 4 ||  (tmpval > 15 && tmpval < 22) || 
					(tmpval == 101 || tmpval == 103 || tmpval == 105 || 
					tmpval == 107 || tmpval == 111 || tmpval == 112 || 
					tmpval == 113 || tmpval == 115)))
					return 3;
				break;
			}
			case 0x0075:
			{
				if (tmpval > 2)
					return 3;
				break;
			}
			case 0x0077:
			{
				if (tmpval > 1)
					return 3;
				break;
			}
			case 0x0078:
			{
				if (tmpval > 1)
					return 3;
				break;
			}
			case 0x0079:
			{
				if (tmpval > 1)
					return 3;
				break;
			}			
		}
	}
	
	memcpy(regsst->val+(startaddr - regsst->addr_bs)*2, para, regcnt*2);
	return 0;	
}

int reg_bankdata_validval(mb_regs_st *regsst, 
	uint16 startaddr, uint8 *para, uint16 regcnt)
{
	int i;
	uint16 tmpval;
	
	for (i = 0; i < regcnt; ++i)
	{
		tmpval = GetNumU16(para+i*2);
		switch (startaddr+i)
		{
			case 0x0060:
			{
				if (tmpval > 1)
					return 3;
				break;
			}
			case 0x0061:
			{
				if (tmpval > 3)
					return 3;
				break;
			}
			case 0x0063:
			{
				if (tmpval > 16)
					return 3;
				break;
			}
		}
	}
	
	memcpy(regsst->val+(startaddr - regsst->addr_bs)*2, para, regcnt*2);
	return 0;	
}

int reg_tagfilter_validval(mb_regs_st *regsst, 
	uint16 startaddr, uint8 *para, uint16 regcnt)
{
	int i;
	uint16 tmpval;
	
	for (i = 0; i < regcnt; ++i)
	{
		tmpval = GetNumU16(para+i*2);
		switch (startaddr+i)
		{
			case 0x0040:
			{
				if (tmpval > 1)
					return 3;
				break;
			}
			case 0x0041:
			{
				if (tmpval > 3)
					return 3;
				break;
			}
			case 0x0043:
			{
				if (tmpval > 432)
					return 3;
				break;
			}
			case 0x0044:
			{
				if (tmpval > 1)
					return 3;
				break;
			}
		}
	}
	
	memcpy(regsst->val+(startaddr - regsst->addr_bs)*2, para, regcnt*2);
	return 0;
}

int reg_powers_validval(mb_regs_st *regsst, 
	uint16 startaddr, uint8 *para, uint16 regcnt)
{
	int i;
	uint16 pwr;
	
	if (startaddr - regsst->addr_bs + regcnt > gAntNumber*2)
		return 2;
	
	for (i = 0; i < regcnt; ++i)
	{
		pwr = GetNumU16(para+i*2);
		if (pwr < gMinTxPower || pwr > gMaxTxPower)
			return 3;
	}
	
	return 0;
}

int reg_reboot_validval(mb_regs_st *regsst, 
	uint16 startaddr, uint8 *para, uint16 regcnt)
{
	uint16 val = GetNumU16(para);
	if (val != 1)
		return 3;
	
	return 0;
}

int reg_perm_save_validval(mb_regs_st *regsst, 
	uint16 startaddr, uint8 *para, uint16 regcnt)
{
	uint16 val = GetNumU16(para);
	if (val != 1)
		return 3;
	
	return 0;
}
void dump_regs_powers(mb_regs_st *regsst)
{
#ifdef _DEBUG
	int i;
	
	TRACE("dump_regs_powers:");
	for (i = 0; i < 32; ++i)
		TRACE("%d ", GetNumU16(regsst->val+i*2));
	TRACE("\n");
#endif
}

READER_ERR set_conf0070(mb_regs_st *regsst, uint8 *rbuf, uint8 *sbuf)
{
	READER_ERR err = MT_OK_ERR;
	uint16 pos = GetNumU16(rbuf+2) - regsst->addr_bs;
	uint16 regcnt;
	int tmpval;
	uint16 regaddr;
	int i;
	
	if (rbuf[1] == 0x10)
		regcnt = GetNumU16(rbuf+4);
	else
		regcnt = 1;
	
	for (i = 0; i < regcnt; ++i)
	{
		tmpval = GetNumU16(regsst->val+ pos*2 + i*2);
		regaddr = regsst->addr_bs + pos + i;
		switch (regaddr)
		{
			case 0x0070:
			{
				err = ParamSet(gRdrHandlePassive, MTR_PARAM_TAGDATA_UNIQUEBYANT, &tmpval);
				break;
			}
			case 0x0071:
			{
				err = ParamSet(gRdrHandlePassive, MTR_PARAM_TAGDATA_UNIQUEBYEMDDATA, &tmpval);			
				break;
			}
			case 0x0072:
			{
				err = ParamSet(gRdrHandlePassive, MTR_PARAM_TAGDATA_RECORDHIGHESTRSSI, &tmpval);
				break;
			}
			case 0x0073:
			{
				err = ParamSet(gRdrHandlePassive, MTR_PARAM_POTL_GEN2_SESSION, &tmpval);
				break;
			}
			case 0x0074:
			{
				if (tmpval == 0xffff)
					tmpval = -1;
				err = ParamSet(gRdrHandlePassive, MTR_PARAM_POTL_GEN2_Q, &tmpval);
				break;
			}
			case 0x0075:
			{
				err = ParamSet(gRdrHandlePassive, MTR_PARAM_POTL_GEN2_TARGET, &tmpval);
				break;
			}
			case 0x0076:
			{
				ParamSet(gRdrHandlePassive, MTR_PARAM_POTL_GEN2_TAGENCODING, &tmpval);
				gPRdrStaSet->protocol.gen2.profile = tmpval;
				break;
			}
			case 0x0077:
			{
				err = ParamSet(gRdrHandlePassive, MTR_PARAM_RF_HOPANTTIME, &tmpval);
				gPRdrStaSet->rf.ant_max_dwell_time = tmpval;
				break;
			}
			case 0x0078:
			{
				gPRdrStaSet->rf.hop_mode = tmpval;
				break;
			}
			case 0x0079:
			{
				gPRdrStaSet->tagops_param.inventory.inv_mode = tmpval;
				break;
			}
			default:
				break;
		}
		SET_DETAIL_ERR(err);
		if (err != MT_OK_ERR)
		{
			TRACE("set regaddr:%04X failed\n", regaddr);
			break;
		}
	}
	
	return err;
}
int mb_get_conf0070_resp(mb_regs_st *regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	READER_ERR err = MT_OK_ERR;
	int i;
	uint16 pos = GetNumU16(rbuf+2) - regsst->addr_bs;
	uint16 regcnt = GetNumU16(rbuf+4);
	uint16 regaddr;
	int tmpval;
	
	for (i = 0; i < regcnt; ++i)
	{
		regaddr = regsst->addr_bs + pos + i;
		switch(regaddr)
		{
			case 0x0070:
			{
				err = ParamGet(gRdrHandlePassive, MTR_PARAM_TAGDATA_UNIQUEBYANT, &tmpval);
				break;
			}
			case 0x0071:
			{
				err = ParamGet(gRdrHandlePassive, MTR_PARAM_TAGDATA_UNIQUEBYEMDDATA, &tmpval);			
				break;
			}
			case 0x0072:
			{
				err = ParamGet(gRdrHandlePassive, MTR_PARAM_TAGDATA_RECORDHIGHESTRSSI, &tmpval);
				break;
			}			
			case 0x0073:
			{
				err = ParamGet(gRdrHandlePassive, MTR_PARAM_POTL_GEN2_SESSION, &tmpval);
				break;
			}
			case 0x0074:
			{
				err = ParamGet(gRdrHandlePassive, MTR_PARAM_POTL_GEN2_Q, &tmpval);
				break;
			}
			case 0x0075:
			{
				err = ParamGet(gRdrHandlePassive, MTR_PARAM_POTL_GEN2_TARGET, &tmpval);
				break;
			}
			case 0x0076:
			{
				ParamGet(gRdrHandlePassive, MTR_PARAM_POTL_GEN2_TAGENCODING, &tmpval);
				break;
			}
			case 0x0077:
			{
				tmpval = gPRdrStaSet->rf.ant_max_dwell_time;
				ParamGet(gRdrHandlePassive, MTR_PARAM_RF_HOPANTTIME, &tmpval);
				break;
			}
			case 0x0078:
			{
				tmpval = gPRdrStaSet->rf.hop_mode;
				break;
			}
			case 0x0079:
			{
				tmpval = gPRdrStaSet->tagops_param.inventory.inv_mode;
				break;
			}
			default:
				break;
		}
		SET_DETAIL_ERR(err);
		if (err == MT_OK_ERR)
			SetNumU16(regsst->val+(regaddr-regsst->addr_bs)*2, tmpval);
		else
		{
			TRACE("mb_get_conf0070_resp regaddr:%04X failed\n", regaddr);
			break;
		}
		
	}
	
	if (err == MT_OK_ERR)
		return mb_common_03_resp(regsst, rbuf, sbuf, nlen);
	else
		return 4;
}

int mb_set_conf0070_resp(mb_regs_st *regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	READER_ERR err = MT_OK_ERR;
	
	err = set_conf0070(regsst, rbuf, sbuf);
	if (err == MT_OK_ERR)
	{
		if (rbuf[1] == 0x06)
			return mb_common_06_resp(regsst, rbuf, sbuf, nlen);
		else
			return mb_common_16_resp(regsst, rbuf, sbuf, nlen);
	}
	else
		return 4;
}

void set_tagfilter(mb_regs_st *regsst)
{
	TagFilter_ST tf;
	int pos = 0;
//	int i;
	uint16 enb = GetNumU16(regsst->val+pos);
	
	if (enb == 0)
	{
		ParamSet(gRdrHandlePassive, MTR_PARAM_TAG_FILTER, NULL);
		gPRdrStaSet->tagops_param.tagfilter.is_tagfilter = 0;
	}
	else
	{
		int maskbytes;
		
		pos += 2;
		tf.bank = GetNumU16(regsst->val+pos);
		pos += 2;
		tf.startaddr = GetNumU16(regsst->val+pos);
		pos += 2;
		tf.flen =  GetNumU16(regsst->val+pos);
		pos += 2;
		tf.isInvert = 1 - GetNumU16(regsst->val+pos);
		pos += 2;
		tf.fdata = regsst->val+pos;
		ParamSet(gRdrHandlePassive, MTR_PARAM_TAG_FILTER, &tf);
		gPRdrStaSet->tagops_param.tagfilter.bank = tf.bank;
		gPRdrStaSet->tagops_param.tagfilter.start = tf.startaddr;
		gPRdrStaSet->tagops_param.tagfilter.mask_len = tf.flen;
		gPRdrStaSet->tagops_param.tagfilter.match = 1 - tf.isInvert;
		maskbytes = tf.flen / 8;
		if (tf.flen % 8 != 0)
			maskbytes++;
		memcpy(gPRdrStaSet->tagops_param.tagfilter.mask, tf.fdata, maskbytes);
		gPRdrStaSet->tagops_param.tagfilter.is_tagfilter = 1;
		TRACE("en:%d,bank:%d,start:%d,flen:%d,match:%d\n", enb, 
			tf.bank, tf.startaddr, tf.flen, 1 - tf.isInvert);
#ifdef _DEBUG
		TRACE("fdata:");
		for (uint16_t i = 0; i < maskbytes; ++i)
			TRACE("%02X ", tf.fdata[i]);
		TRACE("\n");
#endif
	}
}

void set_bankdata(mb_regs_st *regsst)
{
	EmbededData_ST embdata;
	if (GetNumU16(regsst->val) == 0)
	{
		TRACE("disable embdata\n");
		ParamSet(gRdrHandlePassive, MTR_PARAM_TAG_EMBEDEDDATA, NULL);
		gPRdrStaSet->tagops_param.bankdata.is_bankdata = 0;
	}
	else
	{
		embdata.bank = GetNumU16(regsst->val+2);
		embdata.startaddr = GetNumU16(regsst->val+4);
		embdata.bytecnt = GetNumU16(regsst->val+6)*2;
		embdata.accesspwd = gPRdrStaSet->tagops_param.accessop.aespwd;
		ParamSet(gRdrHandlePassive, MTR_PARAM_TAG_EMBEDEDDATA, &embdata);
		
		gPRdrStaSet->tagops_param.bankdata.bank = embdata.bank;
		gPRdrStaSet->tagops_param.bankdata.start = embdata.startaddr;
		gPRdrStaSet->tagops_param.bankdata.blkcnt = embdata.bytecnt / 2;
		gPRdrStaSet->tagops_param.bankdata.is_bankdata = 1;
		
		TRACE("set embdata bank:%d, start:%d, bytecnt:%d, pwd:%08X\n", embdata.bank, 
			embdata.startaddr, embdata.bytecnt, GetNumU32(embdata.accesspwd));
	}
}

int mb_set_bankdata_resp(mb_regs_st *regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	set_bankdata(regsst);
	if (rbuf[1] == 0x06)
		return mb_common_06_resp(regsst, rbuf, sbuf, nlen);
	else
		return mb_common_16_resp(regsst, rbuf, sbuf, nlen);
}

int mb_set_tagfilter_resp(mb_regs_st *regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	set_tagfilter(regsst);
	if (rbuf[1] == 0x06)
		return mb_common_06_resp(regsst, rbuf, sbuf, nlen);
	else
		return mb_common_16_resp(regsst, rbuf, sbuf, nlen);
}

READER_ERR set_tx_powers(mb_regs_st *regsst)
{
	READER_ERR err = MT_OK_ERR;
	int i;
	AntPowerConf pwrs;
	
	pwrs.antcnt = gAntNumber;
	TRACE("set_tx_powers pwrs:");
	for (i = 0; i < gAntNumber; ++i)
	{
		pwrs.Powers[i].antid = i +1;
		pwrs.Powers[i].readPower = GetNumU16(regsst->val+i*4);
		pwrs.Powers[i].writePower = GetNumU16(regsst->val+i*4+2);
		TRACE("%d, %d, %d ", pwrs.Powers[i].antid, pwrs.Powers[i].readPower, 
			pwrs.Powers[i].writePower);
	}
	TRACE("\n");
	err = ParamSet(gRdrHandlePassive, MTR_PARAM_RF_ANTPOWER, &pwrs);
	SET_DETAIL_ERR(err);
	if (err != MT_OK_ERR)
		TRACE("set_tx_powers MTR_PARAM_RF_ANTPOWER failed\n");

	return err;
}

void cp_reg_2_regs(mb_regs_st *regsst, uint8 *rbuf)
{
	uint16 regaddr;
	uint16 regcnt;
	uint8 *para;
	uint16 regval;
	int i;
	
	if (rbuf[1] == 0x10)
	{
		para = rbuf+7;
		regcnt = GetNumU16(rbuf+4);
	}
	else if (rbuf[1] == 0x06)
	{
		para = rbuf+4;
		regcnt = 1;
	}
	regaddr = GetNumU16(rbuf+2);
	
	for (i = 0; i < regcnt; ++i)
	{
		regval = GetNumU16(para+i*2);
		SetNumU16(regsst->val+(regaddr-regsst->addr_bs)*2+i*2, regval);
	}	
}

READER_ERR pack_0020regs_bytes(mb_regs_st *regsst, uint8 *rbuf)
{
	READER_ERR err;
	AntPowerConf pwrs;
	int i;
	
	for (i = 0; i < 16; ++i)
	{
		SetNumU16(regsst->val+i*4, gPRdrStaSet->rf.tx_powers[i].read_power);
		SetNumU16(regsst->val+i*4+2, gPRdrStaSet->rf.tx_powers[i].write_power);
	}
	err = ParamGet(gRdrHandlePassive, MTR_PARAM_RF_ANTPOWER, &pwrs);
	SET_DETAIL_ERR(err);
	if (err == MT_OK_ERR)
	{
		for (i = 0; i < pwrs.antcnt; ++i)
		{
			SetNumU16(regsst->val+i*4, pwrs.Powers[i].readPower);
			SetNumU16(regsst->val+i*4+2, pwrs.Powers[i].writePower);		
		}
		cp_reg_2_regs(regsst, rbuf);
	}

	return err;
}

int mb_get_powers_resp(mb_regs_st *regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	READER_ERR err;
	err = pack_0020regs_bytes(regsst, rbuf);

	if (err != MT_OK_ERR)
		return 4;
	return mb_common_03_resp(regsst, rbuf, sbuf, nlen);
}

int mb_set_powers_resp(mb_regs_st *regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	READER_ERR err;
	
	err = pack_0020regs_bytes(regsst, rbuf);
	if (err == MT_OK_ERR)
	{
		err = set_tx_powers(regsst);
		if (err == MT_OK_ERR)
		{
			if (rbuf[1] == 0x06)
				return mb_common_06_resp(regsst, rbuf, sbuf, nlen);
			else
				return mb_common_16_resp(regsst, rbuf, sbuf, nlen);			
		}
		else
			return 4;
	}

	return 4;
}


int gMbInvTagCnt = 0;

READER_ERR pack_tagbytes(int *nbytes)
{
	int pos = 6;
	int i;
	int batchcnt = 0;
	READER_ERR err = MT_OK_ERR;
	TAGINFO tag;
	
	/*
	current batch byte number(2 bytes)+current batch tags number(2 bytes)+no data(2 bytes)+tag bytes
	*/
	for (i = 0; i < gMbInvTagCnt; ++i)
	{
		err = GetNextTag(gRdrHandlePassive, &tag);
		SET_DETAIL_ERR(err);
		if (err == MT_OK_ERR)
		{
			batchcnt++;
			g_regs_tagbytes->val[pos++] = tag.ReadCnt;
			g_regs_tagbytes->val[pos++] = tag.AntennaID;
			g_regs_tagbytes->val[pos++] = (uint8)((signed char)tag.RSSI);
			g_regs_tagbytes->val[pos++] = (tag.Frequency >> 16) & 0xff;
			g_regs_tagbytes->val[pos++] = (tag.Frequency >> 8) & 0xff;
			g_regs_tagbytes->val[pos++] = (tag.Frequency >> 0) & 0xff;
			g_regs_tagbytes->val[pos++] = tag.protocol;
			memcpy(g_regs_tagbytes->val+pos, tag.Res, 2);
			pos += 2;
			memcpy(g_regs_tagbytes->val+pos, tag.PC, 2);
			pos += 2;
			g_regs_tagbytes->val[pos++] = tag.Epclen;
			memcpy(g_regs_tagbytes->val+pos, tag.EpcId, tag.Epclen);
			pos += tag.Epclen;
			SetNumU16(g_regs_tagbytes->val+pos, tag.EmbededDatalen);
			pos += 2;
			memcpy(g_regs_tagbytes->val+pos, tag.EmbededData, tag.EmbededDatalen);
			pos += tag.EmbededDatalen;
			if (pos >= (250 - gPRdrStaSet->app_init.max_tb_rec_len-14))
					break;
		}
		else
			break;
	}
	if (err == MT_OK_ERR)
	{
		if (batchcnt == 0)
			*nbytes = 0;
		else
			*nbytes = pos-2;
		TRACE("gMbInvTagCnt:%d, batchcnt:%d, nbytes:%d\n", 
			gMbInvTagCnt, batchcnt, *nbytes);
		gMbInvTagCnt -= batchcnt;
		SetNumU16(g_regs_tagbytes->val, *nbytes);
		SetNumU16(g_regs_tagbytes->val+2, batchcnt);
	}
	
	return err;
}

int mb_tagbytes_03_resp(mb_regs_st * regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	int pos = 0;
	int nbytes;
	READER_ERR err = MT_OK_ERR;
	int toNbytes = GetNumU16(rbuf+4)*2;
	
	if (GetNumU16(g_regs_tagbytes->val+2) == 0)
	{
		SetNumU16(g_regs_rdrsta->val, 0x0400);
		return 2;
	}
	if (toNbytes != GetNumU16(g_regs_tagbytes->val))
	{
		return 2;
	}
	
	g_mb_send_buf[pos++] = gModBusAddr;
	g_mb_send_buf[pos++] = 0x03;
	g_mb_send_buf[pos++] = toNbytes;
	memcpy(g_mb_send_buf+pos, regsst->val+2, toNbytes);
	pos += toNbytes;

	err = pack_tagbytes(&nbytes);
	if (err == MT_OK_ERR)
	{
		SetNumU16(g_mb_send_buf+5, nbytes);
		mb_add_crc(sbuf, pos);
		*nlen = pos+2;
		return 0;
	}
	else
		return 4;
}

int mb_apires_03_resp(mb_regs_st * regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	int tagcnt;
//	int batchcnt = 0;
	READER_ERR err = MT_OK_ERR;
	int nbytes;
	uint16 startreg = GetNumU16(rbuf+2);

	TRACE("before TagInventory_Raw:%lld\n", getSysTick());
	if (gPRdrStaSet->tagops_param.inventory.ants_cnt == 0)
	{
		SetNumU16(g_regs_rdrsta->val, 0x07);
		return 4;
	}
	
	err = TagInventory_Raw(gRdrHandlePassive, gPRdrStaSet->tagops_param.inventory.ants, 
		gPRdrStaSet->tagops_param.inventory.ants_cnt, 
		gPRdrStaSet->tagops_param.inventory.cycle, &tagcnt);
	SET_DETAIL_ERR(err);
	if (err == MT_OK_ERR)
	{
		gMbInvTagCnt = tagcnt;
		err = pack_tagbytes(&nbytes);
	}

	TRACE("after TagInventory_Raw:%lld\n", getSysTick());
	if (err == MT_OK_ERR)
	{
//		SetNumU16(regsst->val, batchcnt);
		if (tagcnt == 0)
			SetNumU16(regsst->val, 0);
		else
			SetNumU16(regsst->val, nbytes);
//		SetNumU16(regsst->val+2, 0);
		return mb_common_03_resp(regsst, rbuf, sbuf, nlen);
	}
	else
		return 4;
	
}

int get_temperature_from_data(uint8 *data, uint8 *temper)
{
	int bdalen;
	uint16 metaflag;
	int i = 0;
	uint8 option = data[i++];
			
	if ((option & 0x10) != 0)
	{
		metaflag = (short)(data[i++] << 8);
		metaflag |= data[i++];

		if (0 != (metaflag & 0x0001))
			i++;
		if (0 != (metaflag & 0x0002))
			i++;
		if (0 != (metaflag & 0x0004))
			i++;
		if (0 != (metaflag & 0x0008))
			i += 3;
		if (0 != (metaflag & 0x0010))
			i += 4;
		if (0 != (metaflag & 0x0020))
			i += 2;
		if (0 != (metaflag & 0x0040))
			i++;

		if (0 != (metaflag & 0X0080))
		{
			bdalen = ((data[i] << 8) | data[i + 1]) / 8;
			i += bdalen+2;
		}
	}	

	memcpy(temper, data+i, 2);
	if (temper[0] == 0x00 && temper[1] == 0x00)
		return -1;
	return 0;
}

int mb_short_inv_03_resp(mb_regs_st * regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	READER_ERR err = MT_OK_ERR;
	TAGINFO tag;
	int tagcnt;
	int pos = 2;
	int i;
	int realcnt = 0;
	int infobytescnt = 0;
	int curtagbytes = 0;
	int invdur = gPRdrStaSet->tagops_param.inventory.ants_cnt * 
		gPRdrStaSet->tagops_param.inventory.cycle;
	
	if (gPRdrStaSet->tagops_param.inventory.ants_cnt == 0)
	{
//		TRACE("!!!!! if (gPRdrStaSet->tagops_param.inventory.ants_cnt == 0)\n");
		SetNumU16(g_regs_rdrsta->val, 0x07);
		return 4;
	}
	
	if ((gPRdrStaSet->tagops_param.mb_sinv_tag_fmt & 0x01) == 0x01)
		infobytescnt += 2;
	if ((gPRdrStaSet->tagops_param.mb_sinv_tag_fmt & 0x02) == 0x02)
		infobytescnt += 2;
	if ((gPRdrStaSet->tagops_param.mb_sinv_tag_fmt & 0x04) == 0x04)
		infobytescnt += 4;
	if ((gPRdrStaSet->tagops_param.mb_sinv_tag_fmt & 0x08) == 0x08)
		infobytescnt += 2;
	if ((gPRdrStaSet->tagops_param.mb_sinv_tag_fmt & 0x10) == 0x10)
		infobytescnt += 2;
	if ((gPRdrStaSet->tagops_param.mb_sinv_tag_fmt & 0x20) == 0x20)
		infobytescnt += 2;
		
	err = TagInventory_Raw(gRdrHandlePassive, gPRdrStaSet->tagops_param.inventory.ants, 
		gPRdrStaSet->tagops_param.inventory.ants_cnt, invdur, &tagcnt);
	SET_DETAIL_ERR(err);
//	TRACE("!!!!! after TagInventory_Raw\n");
	if(err != MT_OK_ERR)
		return 4;
	
	memcpy(sbuf, rbuf, 2);
	sbuf[pos++] = regsst->mwdcnt * 2;
	memset(sbuf+pos, 0, regsst->mwdcnt*2);
	pos += 2;
//	TRACE("!!!!! before GetNextTag tagcnt:%d\n", tagcnt);
	for (i = 0; i < tagcnt; ++i)
	{
		err = GetNextTag(gRdrHandlePassive, &tag);
		SET_DETAIL_ERR(err);
		if (err == MT_OK_ERR)
		{
			curtagbytes = infobytescnt;
			if ((gPRdrStaSet->tagops_param.mb_sinv_tag_fmt & 0x20) == 0x20)
				curtagbytes += tag.EmbededDatalen;
			if (pos + tag.Epclen+curtagbytes > 253)
			{
				TRACE("if (pos + tag.Epclen+curtagbytes> 253)\n");
				break;
			}
			realcnt++;
			
			if ((gPRdrStaSet->tagops_param.mb_sinv_tag_fmt & 0x01) == 0x01)
			{
				sbuf[pos++] = tag.AntennaID;
				sbuf[pos++] = tag.Epclen;
			}
			if ((gPRdrStaSet->tagops_param.mb_sinv_tag_fmt & 0x02) == 0x02)
			{
				sbuf[pos++] = tag.ReadCnt;
				sbuf[pos++] = (uint8)((signed char)tag.RSSI);
			}
			if ((gPRdrStaSet->tagops_param.mb_sinv_tag_fmt & 0x04) == 0x04)
			{
				sbuf[pos++] = tag.protocol;
				sbuf[pos++] = (tag.Frequency >> 16) & 0xff;
				sbuf[pos++] = (tag.Frequency >> 8) & 0xff;
				sbuf[pos++] = (tag.Frequency >> 0) & 0xff;
			}
			if ((gPRdrStaSet->tagops_param.mb_sinv_tag_fmt & 0x08) == 0x08)
			{
				memcpy(sbuf+pos, tag.Res, 2);
				pos += 2;
			}
			if ((gPRdrStaSet->tagops_param.mb_sinv_tag_fmt & 0x10) == 0x10)
			{
				memcpy(sbuf+pos, tag.PC, 2);
				pos += 2;
			}
			
			memcpy(sbuf+pos, tag.EpcId, tag.Epclen);
			pos += tag.Epclen;
			
			if ((gPRdrStaSet->tagops_param.mb_sinv_tag_fmt & 0x20) == 0x20)
			{
				SetNumU16(sbuf+pos, tag.EmbededDatalen);
				pos += 2;
				memcpy(sbuf+pos, tag.EmbededData, tag.EmbededDatalen);
				pos += tag.EmbededDatalen;
			}
		}
		else
			return 4;
	}

	SetNumU16(sbuf+3, realcnt);
	mb_add_crc(sbuf, regsst->mwdcnt * 2+3);
	*nlen = regsst->mwdcnt * 2+5;
	return 0;
}

int mb_yd_inv_temper_op_03_resp(mb_regs_st * regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	READER_ERR err = MT_OK_ERR;
	uint8 ret_data[100];
	int datalen = 0;
	int tagcnt;
	TAGINFO tag;
	TagFilter_ST tf;
	TagFilter_ST tfold;
	uint8 fdataold[256];
	int invdur;
	int pos = 2;
	int succnt = 0;
	int j;
	int c;
	int mbret = 4;
	int opant;
	int tmpant;
	uint8 temper[2];
	
	tf.bank = 1;
	tf.startaddr = 32;
	tf.isInvert = 0;
	invdur = gPRdrStaSet->tagops_param.inventory.ants_cnt * 
		gPRdrStaSet->tagops_param.inventory.cycle;	
	tfold.fdata = fdataold;
	
	if (gPRdrStaSet->tagops_param.inventory.ants_cnt == 0)
	{
		SetNumU16(g_regs_rdrsta->val, 0x07);
		return 4;
	}

	err = TagInventory_Raw(gRdrHandlePassive, gPRdrStaSet->tagops_param.inventory.ants, 
		gPRdrStaSet->tagops_param.inventory.ants_cnt, invdur, &tagcnt);
	SET_DETAIL_ERR(err);
	if(err != MT_OK_ERR)
		return 4;
	
	memcpy(sbuf, rbuf, 2);
	sbuf[pos++] = regsst->mwdcnt * 2;
	memset(sbuf+pos, 0, 125*2);
	pos += 2;
	
	ParamGet(gRdrHandlePassive, MTR_PARAM_TAG_FILTER, &tfold);
	TRACE("tagcnt:%d\n", tagcnt);
	for (c = 0; c < tagcnt; ++c)
	{
		err = GetNextTag(gRdrHandlePassive, &tag);
		SET_DETAIL_ERR(err);
		if(err != MT_OK_ERR)
			goto FIN;
		
		if (pos + tag.Epclen+4 > 253)
		{
			TRACE("if (pos + tag.Epclen+4 > 253)\n");
			break;
		}
		
		tf.fdata = tag.EpcId;
		tf.flen = tag.Epclen * 8;
		ParamSet(gRdrHandlePassive, MTR_PARAM_TAG_FILTER, &tf);
		opant = tag.AntennaID;
		
		err = ReadTagTemperature(gRdrHandlePassive, opant, 
			regsst->tagbank, 0x7F, 1, gPRdrStaSet->tagops_param.accessop.timeout+100, 0, 100, 
			0, NULL, ret_data, &datalen);
		
		if (err == MT_OK_ERR)
		{
			if (get_temperature_from_data(ret_data, temper) != 0)
				err = MT_CMD_FAILED_ERR;
		}
		
//		err = GetTagData(gRdrHandlePassive, opant, 1, 2, 6, ret_data, NULL, 1100);
		if(err != MT_OK_ERR && gPRdrStaSet->tagops_param.inventory.ants_cnt > 1)
		{
			for (j = 0; j < gPRdrStaSet->tagops_param.inventory.ants_cnt; ++j)
			{			
				if (gPRdrStaSet->tagops_param.inventory.ants[j] != opant)
				{
					tmpant = gPRdrStaSet->tagops_param.inventory.ants[j];
					
					err = ReadTagTemperature(gRdrHandlePassive, tmpant, regsst->tagbank, 0x7F, 
						1, gPRdrStaSet->tagops_param.accessop.timeout+100, 0, 100, 
						0, NULL, ret_data, &datalen);
					
					TRACE("c:%d, tmpant:%d\n", c, tmpant);
//					err = GetTagData(gRdrHandlePassive, tmpant, 1, 2, 6, ret_data, NULL, 1100);
				}
				if(err == MT_OK_ERR)
				{
					if (get_temperature_from_data(ret_data, temper) == 0)
					{
						opant = tmpant;
						break;
					}
					else
						err = MT_CMD_FAILED_ERR;
				}
			}
		}

		if(err == MT_OK_ERR)
		{
			memcpy(sbuf+pos, temper, 2);
			pos += 2;
			memcpy(sbuf+pos, tag.PC, 2);
			pos += 2;
			sbuf[pos++] = opant;
			
			/*
			memcpy(sbuf+pos, ret_data, 2);
			pos += 2;
			sbuf[pos++] = opant;
			*/
		}
		else
		{
			sbuf[pos++] = 0x00;
			sbuf[pos++] = 0x00;
			memcpy(sbuf+pos, tag.PC, 2);
			pos += 2;
			sbuf[pos++] = 0x00;
			TRACE("----- c:%d failed\n", c);
		}
		
		sbuf[pos++] = tag.Epclen;
		memcpy(sbuf+pos, tag.EpcId, tag.Epclen);
		pos += tag.Epclen;
		succnt++;
	}
	
	mbret = 0;
	SetNumU16(sbuf+3, succnt);
	mb_add_crc(sbuf, regsst->mwdcnt * 2+3);
	*nlen = regsst->mwdcnt * 2+5;
	
FIN:
	if (tfold.flen == 0)
		ParamSet(gRdrHandlePassive, MTR_PARAM_TAG_FILTER, NULL);
	else
		ParamSet(gRdrHandlePassive, MTR_PARAM_TAG_FILTER, &tfold);
	return mbret;
}

int mb_yd_temperature_op_03_resp(mb_regs_st * regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	READER_ERR err = MT_OK_ERR;
	uint8 ret_data[100];
	int datalen = 0;
	
	if (gPRdrStaSet->tagops_param.accessop.ant == 0)
	{
		SetNumU16(g_regs_rdrsta->val, 0x07);
		return 4;
	}
	
	err = ReadTagTemperature(gRdrHandlePassive, gPRdrStaSet->tagops_param.accessop.ant, 
		regsst->tagbank, 0x7F, 1, gPRdrStaSet->tagops_param.accessop.timeout, 0, 100, 
		0, NULL, ret_data, &datalen);
	SET_DETAIL_ERR(err);
	
	if(err == MT_OK_ERR)
	{
		int taglen;
		int bdalen;
		uint16 metaflag;
		int i = 0;
		uint8 option = ret_data[i++];
		int pos = 2;
		
		if ((option & 0x10) != 0)
		{
			metaflag = (short)(ret_data[i++] << 8);
			metaflag |= ret_data[i++];

			if (0 != (metaflag & 0x0001))
				i++;
			if (0 != (metaflag & 0x0002))
				i++;
			if (0 != (metaflag & 0x0004))
				i++;
			if (0 != (metaflag & 0x0008))
				i += 3;
			if (0 != (metaflag & 0x0010))
				i += 4;
			if (0 != (metaflag & 0x0020))
				i += 2;
			if (0 != (metaflag & 0x0040))
				i++;

			if (0 != (metaflag & 0X0080))
			{
				bdalen = ((ret_data[i] << 8) | ret_data[i + 1]) / 8;
				i += bdalen+2;
			}
		}
		memset(sbuf+3, 0, 80);
		memcpy(sbuf+3, ret_data+i, 2);
		i += 2;
		taglen = ret_data[i++];
		memcpy(sbuf+5, ret_data+i, 2);
		i += 2;
		SetNumU16(sbuf+7, taglen - 4);
		memcpy(sbuf+9, ret_data+i, taglen - 4);
		
		memcpy(sbuf, rbuf, 2);
		sbuf[pos++] = regsst->mwdcnt * 2;;
		pos += regsst->mwdcnt * 2;;
		mb_add_crc(sbuf, pos);
		*nlen = pos+2;
		return 0;
	}
	else
		return 4;
}

int mb_tagmemop_03_resp(mb_regs_st * regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	int pos = 2;
	READER_ERR err = MT_OK_ERR;
	uint8 *aespwd;
	uint16 startreg = GetNumU16(rbuf+2);
	uint16 regcnt = GetNumU16(rbuf+4);
	
	if (GetNumU32(gPRdrStaSet->tagops_param.accessop.aespwd) == 0)
		aespwd = NULL;
	else
		aespwd = gPRdrStaSet->tagops_param.accessop.aespwd;
	TRACE("mb_tagmemop_03_resp tagop_ant:%d,bank:%d,startaddr:%d,blkcnt:%d,timeout:%d\n", 
		gPRdrStaSet->tagops_param.accessop.ant, regsst->tagbank, startreg-regsst->addr_bs, 
		regcnt, gPRdrStaSet->tagops_param.accessop.timeout);
	if (gPRdrStaSet->tagops_param.accessop.ant == 0)
	{
		SetNumU16(g_regs_rdrsta->val, 0x07);
		return 4;
	}
	
	err = GetTagData(gRdrHandlePassive, gPRdrStaSet->tagops_param.accessop.ant, regsst->tagbank,
		startreg-regsst->addr_bs, regcnt, sbuf+3, aespwd, gPRdrStaSet->tagops_param.accessop.timeout);
	SET_DETAIL_ERR(err);
	if (err == MT_OK_ERR)
	{
		memcpy(sbuf, rbuf, 2);
		sbuf[pos++] = regcnt*2;
		pos += regcnt*2;
		mb_add_crc(sbuf, pos);
		*nlen = pos+2;
		return 0;
	}
	else
		return 4;
}

int mb_tagmemop_16_resp(mb_regs_st * regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	READER_ERR err = MT_OK_ERR;
	uint8 *aespwd;
	uint16 startreg = GetNumU16(rbuf+2);
	
	if (GetNumU32(gPRdrStaSet->tagops_param.accessop.aespwd) == 0)
		aespwd = NULL;
	else
		aespwd = gPRdrStaSet->tagops_param.accessop.aespwd;
	
	TRACE("mb_tagmemop_16_resp tagop_ant:%d,bank:%d,startaddr:%d,nbyte:%d,timeout:%d\n", 
		gPRdrStaSet->tagops_param.accessop.ant, regsst->tagbank, startreg-regsst->addr_bs, rbuf[6], 
		gPRdrStaSet->tagops_param.accessop.timeout);
	if (gPRdrStaSet->tagops_param.accessop.ant == 0)
	{
		SetNumU16(g_regs_rdrsta->val, 0x07);
		return 4;
	}
	
	err = WriteTagData(gRdrHandlePassive, gPRdrStaSet->tagops_param.accessop.ant, 
		regsst->tagbank, startreg-regsst->addr_bs, rbuf+7, rbuf[6], aespwd, 
		gPRdrStaSet->tagops_param.accessop.timeout);
	SET_DETAIL_ERR(err);
	if (err == MT_OK_ERR)
	{
		memcpy(sbuf, rbuf, 6);
		mb_add_crc(sbuf, 6);
		*nlen = 8;
		return 0;
	}
	else
		return 4;
}

int mb_gpiget_03_resp(mb_regs_st * regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	uint16 val = 0;
	int i;
	
	for (i = 0; i < 4; ++i)
		val |= gpi_get(i+1) << i;

	memcpy(sbuf, rbuf, 2);
	sbuf[2] = 2;
	SetNumU16(sbuf+3, val);
	mb_add_crc(sbuf, 5);
	*nlen = 7;
	return 0;	
}

void set_gpostates(uint8 *rbuf, uint8 *sbuf)
{
	int i;
	uint8 b1 = rbuf[4];
	uint8 b2 = rbuf[5];
	
	for (i = 0; i < 4; ++i)
	{
		if (((b1 >> i) & 0x01) == 0x01)
			gpo_set(i+1, (b2 >> i) & 0x01);
	}
}

int mb_gposet_resp(mb_regs_st * regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	set_gpostates(rbuf, sbuf);
	if (rbuf[1] == 0x06)
		return mb_common_06_resp(regsst, rbuf, sbuf, nlen);
	else
		return mb_common_16_resp(regsst, rbuf, sbuf, nlen);	
}

int mb_reboot_resp(mb_regs_st * regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	if (rbuf[1] == 0x06)
		mb_common_06_resp(regsst, rbuf, sbuf, nlen);
	else
		mb_common_16_resp(regsst, rbuf, sbuf, nlen);

	return 10;
}

int mb_perm_save_resp(mb_regs_st * regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	int ret;
	
	ret = SaveCurStaticSettings();
	if (ret != 0)
		return 4;
	else
	{
		if (rbuf[1] == 0x06)
			mb_common_06_resp(regsst, rbuf, sbuf, nlen);
		else
			mb_common_16_resp(regsst, rbuf, sbuf, nlen);
		return 0;
	}
}

int mb_getconnants_03_resp(mb_regs_st * regsst, 
	uint8 *rbuf, uint8 *sbuf, uint8 *nlen)
{
	READER_ERR err = MT_OK_ERR;
	ConnAnts_ST ants;
	
	err = ParamGet(gRdrHandlePassive, MTR_PARAM_READER_CONN_ANTS, &ants);
	SET_DETAIL_ERR(err);
	if (err == MT_OK_ERR)
	{
		uint16 val = 0;
		int i;
		
		for (i = 0; i < ants.antcnt; ++i)
			val |= 1  << (ants.connectedants[i] - 1);
		
		memcpy(sbuf, rbuf, 2);
		sbuf[2] = 2;
		SetNumU16(sbuf+3, val);
		mb_add_crc(sbuf, 5);
		*nlen = 7;
	}
	else
		return 4;
	
	return 0;
}

void mb_copy_uart_conf()
{
	SetNumU16(g_reg_head->val, gPMbCurUartSet->address);
	SetNumU16(g_reg_head->val+2, uartconf2regval(gPMbCurUartSet));
}

void init_modbus()
{
	int i;
	mb_regs_st *regsst;
	mb_regs_st *next;

	g_mb_regvals_bak = malloc_hexp(64);
	g_mb_tcp_sendbuf = malloc_hexp(300);
	g_mb_send_buf = g_mb_tcp_sendbuf+6;

	regsst = malloc_hexp(sizeof(mb_regs_st));
	memset(regsst, 0, sizeof(mb_regs_st));
	g_reg_head = regsst;
	regsst->addr_bs = 0x0000;
	regsst->val = malloc_hexp(16);
	regsst->mwdcnt = 8;
//	SetNumU16(regsst->val, gPRdrStaSet->uart.address);
//	SetNumU16(regsst->val+2, uartconf2regval(&gPRdrStaSet->uart));
	SetNumU16(regsst->val+4, gPRdrStaSet->tagops_param.inventory.cycle);
	{
		uint16 val_ = 0;
		for (i = 0; i < gPRdrStaSet->tagops_param.inventory.ants_cnt; ++i)
			val_ |= 1 << (gPRdrStaSet->tagops_param.inventory.ants[i]-1);
		SetNumU16(regsst->val+6, val_);
	}
	SetNumU16(regsst->val+8, gPRdrStaSet->tagops_param.accessop.ant);
	SetNumU16(regsst->val+10, gPRdrStaSet->tagops_param.accessop.timeout);
	memcpy(regsst->val+12, gPRdrStaSet->tagops_param.accessop.aespwd, 4);
	regsst->isasepart = 1;
//	regsst->canbak = 1;
	regsst->cmd_03_handler = mb_common_03_resp;
	regsst->cmd_06_handler = mb_common_06_resp;
	regsst->cmd_16_handler = mb_common_16_resp;
	regsst->valid_regval = reg_conf0000_validval;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;
	
	memset(regsst, 0, sizeof(mb_regs_st));
	regsst->addr_bs = 0x0020;
	regsst->val = malloc_hexp(64);
	regsst->mwdcnt = 32;
	regsst->isasepart = 1;
//	regsst->canbak = 1;
//	memset(regsst->val, 0, 64);
	ParamGet(gRdrHandlePassive, MTR_PARAM_RF_MINPOWER, &gMinTxPower);
	ParamGet(gRdrHandlePassive, MTR_PARAM_RF_MAXPOWER, &gMaxTxPower);
	regsst->cmd_03_handler = mb_get_powers_resp;
	regsst->cmd_06_handler = mb_set_powers_resp;
	regsst->cmd_16_handler = mb_set_powers_resp;
	regsst->valid_regval = reg_powers_validval;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;
	
	memset(regsst, 0, sizeof(mb_regs_st));
	regsst->addr_bs = 0x0040;
	regsst->val = malloc_hexp(64);
	regsst->mwdcnt = 32;
	memset(regsst->val, 0, 64);
	regsst->isasepart = 1;
	if (gPRdrStaSet->tagops_param.tagfilter.is_tagfilter == 1)
	{
		int maskbyteslen;
		int tfpos = 0;
		SetNumU16(regsst->val+tfpos, 1);
		tfpos += 2;
		SetNumU16(regsst->val+tfpos, gPRdrStaSet->tagops_param.tagfilter.bank);
		tfpos += 2;
		SetNumU16(regsst->val+tfpos, gPRdrStaSet->tagops_param.tagfilter.start);
		tfpos += 2;
		SetNumU16(regsst->val+tfpos, gPRdrStaSet->tagops_param.tagfilter.mask_len);
		tfpos += 2;
		SetNumU16(regsst->val+tfpos, gPRdrStaSet->tagops_param.tagfilter.match);
		tfpos += 2;
		maskbyteslen = gPRdrStaSet->tagops_param.tagfilter.mask_len / 8;
		if (gPRdrStaSet->tagops_param.tagfilter.mask_len % 8 != 0)
			maskbyteslen++;
		memcpy(regsst->val+tfpos, gPRdrStaSet->tagops_param.tagfilter.mask, maskbyteslen);
	}
	regsst->cmd_03_handler = mb_common_03_resp;
	regsst->cmd_06_handler = mb_set_tagfilter_resp;
	regsst->cmd_16_handler = mb_set_tagfilter_resp;
	regsst->valid_regval = reg_tagfilter_validval;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;
	
	memset(regsst, 0, sizeof(mb_regs_st));
	regsst->addr_bs = 0x0060;
	regsst->val = malloc_hexp(8);
	regsst->mwdcnt = 4;
	SetNumU16(regsst->val, 0);
	regsst->isasepart = 1;
	memset(regsst->val, 0, 8);
	if (gPRdrStaSet->tagops_param.bankdata.is_bankdata == 1)
	{
		int embpos = 0;
		SetNumU16(regsst->val+embpos, 1);
		embpos += 2;
		SetNumU16(regsst->val+embpos, gPRdrStaSet->tagops_param.bankdata.bank);
		embpos += 2;
		SetNumU16(regsst->val+embpos, gPRdrStaSet->tagops_param.bankdata.start);
		embpos += 2;
		SetNumU16(regsst->val+embpos, gPRdrStaSet->tagops_param.bankdata.blkcnt);		
	}
	regsst->cmd_03_handler = mb_common_03_resp;
	regsst->cmd_06_handler = mb_set_bankdata_resp;
	regsst->cmd_16_handler = mb_set_bankdata_resp;
	regsst->valid_regval = reg_bankdata_validval;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;
	
	memset(regsst, 0, sizeof(mb_regs_st));
	regsst->addr_bs = 0x0070;
	regsst->val = malloc_hexp(20);
	regsst->mwdcnt = 10;
	regsst->isasepart = 1;
	regsst->cmd_03_handler = mb_get_conf0070_resp;
	regsst->cmd_06_handler = mb_set_conf0070_resp;
	regsst->cmd_16_handler = mb_set_conf0070_resp;
	regsst->valid_regval = reg_conf0070_validval;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;

	memset(regsst, 0, sizeof(mb_regs_st));
	regsst->addr_bs = 0x0080;
	regsst->mwdcnt = 1;
	regsst->cmd_03_handler = mb_gpiget_03_resp;
	regsst->cmd_06_handler = NULL;
	regsst->cmd_16_handler = NULL;
	regsst->valid_regval = NULL;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;

	memset(regsst, 0, sizeof(mb_regs_st));
	regsst->addr_bs = 0x0082;
	regsst->mwdcnt = 1;
	regsst->cmd_03_handler = NULL;
	regsst->cmd_06_handler = mb_gposet_resp;
	regsst->cmd_16_handler = mb_gposet_resp;
	regsst->valid_regval = NULL;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;

	memset(regsst, 0, sizeof(mb_regs_st));
	regsst->addr_bs = 0x0084;
	regsst->mwdcnt = 1;
	regsst->cmd_03_handler = mb_getconnants_03_resp;
	regsst->cmd_06_handler = NULL;
	regsst->cmd_16_handler = NULL;
	regsst->valid_regval = NULL;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;
	
	memset(regsst, 0, sizeof(mb_regs_st));
	regsst->addr_bs = 0x0086;
	regsst->mwdcnt = 1;
	regsst->cmd_03_handler = NULL;
	regsst->cmd_06_handler = mb_reboot_resp;
	regsst->cmd_16_handler = mb_reboot_resp;
	regsst->valid_regval = reg_reboot_validval;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;
	
	memset(regsst, 0, sizeof(mb_regs_st));
	g_regs_rdrsta = regsst;
	regsst->addr_bs = 0x0088;
	regsst->mwdcnt = 1;
	regsst->val = malloc_hexp(2);
	memset(regsst->val, 0, 2);
	regsst->cmd_03_handler = mb_common_03_resp;
	regsst->cmd_06_handler = NULL;
	regsst->cmd_16_handler = NULL;
	regsst->valid_regval = NULL;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;
	
	memset(regsst, 0, sizeof(mb_regs_st));
	regsst->addr_bs = 0x018E;
	regsst->mwdcnt = 1;
	regsst->cmd_03_handler = NULL;
	regsst->cmd_06_handler = mb_perm_save_resp;
	regsst->cmd_16_handler = mb_perm_save_resp;
	regsst->valid_regval = reg_perm_save_validval;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;
	
	memset(regsst, 0, sizeof(mb_regs_st));
	regsst->addr_bs = 0x0190;
	regsst->val = malloc_hexp(2);
	regsst->mwdcnt = 1;
	memset(regsst->val, 0, 2);
	regsst->cmd_03_handler = mb_apires_03_resp;
	regsst->cmd_06_handler = NULL;
	regsst->cmd_16_handler = NULL;
	regsst->valid_regval = NULL;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;
	
	memset(regsst, 0, sizeof(mb_regs_st));
	g_regs_tagbytes = regsst;
	regsst->addr_bs = 0x0200;
	regsst->val = malloc_hexp(250);
	regsst->mwdcnt = 125;
	regsst->isasepart = 1;
	memset(regsst->val, 0, 250);
	regsst->cmd_03_handler = mb_tagbytes_03_resp;
	regsst->cmd_06_handler = NULL;
	regsst->cmd_16_handler = NULL;
	regsst->valid_regval = NULL;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;
	
	memset(regsst, 0, sizeof(mb_regs_st));
	regsst->addr_bs = 0x0300;
	regsst->mwdcnt = 4;
	regsst->tagbank = 0;
	regsst->isasepart = 1;
	regsst->cmd_03_handler = mb_tagmemop_03_resp;
	regsst->cmd_06_handler = NULL;
	regsst->cmd_16_handler = mb_tagmemop_16_resp;
	regsst->valid_regval = NULL;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;
	
	memset(regsst, 0, sizeof(mb_regs_st));
	regsst->addr_bs = 0x0310;
	regsst->mwdcnt = 32;
	regsst->tagbank = 1;
	regsst->isasepart = 1;
	regsst->cmd_03_handler = mb_tagmemop_03_resp;
	regsst->cmd_06_handler = NULL;
	regsst->cmd_16_handler = mb_tagmemop_16_resp;
	regsst->valid_regval = NULL;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;
	
	memset(regsst, 0, sizeof(mb_regs_st));
	regsst->addr_bs = 0x0340;
	regsst->mwdcnt = 32;
	regsst->tagbank = 2;
	regsst->isasepart = 1;
	regsst->cmd_03_handler = mb_tagmemop_03_resp;
	regsst->cmd_06_handler = NULL;
	regsst->cmd_16_handler = mb_tagmemop_16_resp;
	regsst->valid_regval = NULL;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;
	
	memset(regsst, 0, sizeof(mb_regs_st));
	regsst->addr_bs = 0x0400;
	regsst->mwdcnt = 512;
	regsst->tagbank = 3;
	regsst->isasepart = 1;
	regsst->cmd_03_handler = mb_tagmemop_03_resp;
	regsst->cmd_06_handler = NULL;
	regsst->cmd_16_handler = mb_tagmemop_16_resp;
	regsst->valid_regval = NULL;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;
	
	memset(regsst, 0, sizeof(mb_regs_st));
	regsst->addr_bs = 0x0600;
	regsst->mwdcnt = 125;
	regsst->tagbank = 3;
	regsst->isasepart = 0;
	regsst->cmd_03_handler = mb_yd_inv_temper_op_03_resp;
	regsst->cmd_06_handler = NULL;
	regsst->cmd_16_handler = NULL;
	regsst->valid_regval = NULL;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;
	
	memset(regsst, 0, sizeof(mb_regs_st));
	regsst->addr_bs = 0x0680;
	regsst->mwdcnt = 40;
	regsst->tagbank = 3;
	regsst->isasepart = 0;
	regsst->cmd_03_handler = mb_yd_temperature_op_03_resp;
	regsst->cmd_06_handler = NULL;
	regsst->cmd_16_handler = NULL;
	regsst->valid_regval = NULL;
	next = malloc_hexp(sizeof(mb_regs_st));
	regsst->next = next;
	regsst = next;
	
	memset(regsst, 0, sizeof(mb_regs_st));
	regsst->addr_bs = 0x0B00;
	regsst->mwdcnt = 125;
	regsst->tagbank = 0;
	regsst->isasepart = 0;
	regsst->cmd_03_handler = mb_short_inv_03_resp;
	regsst->cmd_06_handler = NULL;
	regsst->cmd_16_handler = NULL;
	regsst->valid_regval = NULL;
	regsst->next = NULL;
}

void cont_write(int s, uint8 *buf, uint32 len)
{
//	int i;
	
	if (s == COMMON_INTERFACE_UART2 || s == COMMON_INTERFACE_UART3)
	{
		osKernelLock();
		write(s, buf, len);
		osKernelUnlock();
#ifdef _DEBUG		
		TRACE("uart send resp:");
		for (uint16_t i = 0; i < len; ++i)
			TRACE("%02X ", buf[i]);
		TRACE("\n");
#endif
	}
	else
	{
		SetNumU16(g_mb_tcp_sendbuf, gModtcpSeriNum);
		g_mb_tcp_sendbuf[2] = 0x00;
		g_mb_tcp_sendbuf[3] = 0x00;
		SetNumU16(g_mb_tcp_sendbuf+4, len-2);
		write(s, g_mb_tcp_sendbuf, len+4);
#ifdef _DEBUG
		TRACE("tcp send resp:");
		for (uint16_t i = 0; i < len+4; ++i)
			TRACE("%02X ", g_mb_tcp_sendbuf[i]);
		TRACE("\n");
#endif
	}
}

void modbus_func(int fd, uint8 *rbuf)
{
	int mberr = 0;
	uint8 nresp = 0;
	int ret;
	int dlen;
	uint16 crc;
	mb_regs_st *regsst;
	int regcnt;
	uint16 regaddr;
//	int i;
	
	if (!(fd == COMMON_INTERFACE_UART2 || fd == COMMON_INTERFACE_UART3))
	{
		int nlast;
		nlast = GetNumU16(rbuf+4);
		if (read_n(fd, rbuf+6, nlast) != nlast)
		{
			TRACE("modbus_tcp read_n nlast:%d error\n", nlast);
			goto FIN;
		}
#ifdef _DEBUG
		TRACE("tcp recv cmd:");
		for (uint16_t i = 0; i < nlast+6; ++i)
			TRACE("%02X ", rbuf[i]);
		TRACE("\n");
#endif		
		gModtcpSeriNum = GetNumU16(rbuf);
		memmove(rbuf, rbuf+6, nlast);
		gModBusAddr = rbuf[0];
	}
	else
	{
		if (rbuf[0] != gPMbCurUartSet->address)
		{
			if (rbuf[0] != 0x00)
				goto FIN;
		}

		if (rbuf[1] == 0x03 || rbuf[1] == 0x06)
		{
			ret = read_n(fd, rbuf+3, 5);
			dlen = 6;
		}
		else if (rbuf[1] == 0x10)
			ret = read_n(fd, rbuf+3, 4);
		else
		{
			sleep_ms(50);
			ioctl(fd, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
			nresp = mb_common_err_resp(rbuf[1], 0x01, g_mb_send_buf);
			goto FIN;
		}
		
		if (ret <= 0)
		{
			TRACE("modbus_rtu recv second band failed\n");
			goto FIN;
		}
		else
		{
			if (rbuf[1] == 0x10)
			{
				ret = read_n(fd, rbuf+7, rbuf[6]+2);
				dlen = rbuf[6]+7;
				if (ret <= 0)
				{
					TRACE("modbus_rtu recv 10 cmd last bytes failed\n");
					goto FIN;
				}
			}
		}
		
		crc = crc_Msg(rbuf, dlen);
		if (((rbuf[dlen] << 8) | rbuf[dlen+1]) != crc)
		{
			nresp = mb_common_err_resp(rbuf[1], 0x04, g_mb_send_buf);
			TRACE("modbus_rtu crc error\n");
			goto FIN;
		}
		gModBusAddr = gPMbCurUartSet->address;
#ifdef _DEBUG
		TRACE("uart recv cmd:");
		for (uint16_t i = 0; i < dlen+2; ++i)
			TRACE("%02X ", rbuf[i]);
		TRACE("\n");
#endif
	}
	
	if (rbuf[1] == 0x03 || rbuf[1] == 0x10)
		regcnt = GetNumU16(rbuf+4);
	else if (rbuf[1] == 0x06)
		regcnt = 1;
	
	regaddr = GetNumU16(rbuf+2);
	
	regsst = find_regs(regaddr, regcnt);
	if (regsst == NULL)
	{
		nresp = mb_common_err_resp(rbuf[1], 0x02, g_mb_send_buf);
		TRACE("modbus_rtu regst == NULL\n");
		goto FIN;
	}
	
//	if (regsst->canbak == 1)
//		memcpy(g_mb_regvals_bak, regsst->val, regsst->mwdcnt*2);
	switch(rbuf[1]) 
	{
		case 0x03:
			if(regsst->cmd_03_handler == NULL)
				mberr = 0x01;
			else
				mberr = regsst->cmd_03_handler(regsst, rbuf, g_mb_send_buf, &nresp);
			break;
		case 0x06:
			if(regsst->cmd_06_handler == NULL)
				mberr = 0x01;
			else
			{
				if (regsst->valid_regval != NULL)
					mberr = regsst->valid_regval(regsst, regaddr, rbuf+4, 1);				
				if (mberr == 0)
					mberr = regsst->cmd_06_handler(regsst, rbuf, g_mb_send_buf, &nresp);				
			}
			break;
		case 0x10:
			if(regsst->cmd_16_handler == NULL)
				mberr = 0x01;
			else
			{
				if (regsst->valid_regval != NULL)
					mberr = regsst->valid_regval(regsst, regaddr, rbuf+7, regcnt);
				if (mberr == 0)				
					mberr = regsst->cmd_16_handler(regsst, rbuf, g_mb_send_buf, &nresp);				
			}
			break;
		default:
			mberr = 0x01;
			break;
	}
	
	if (mberr != 0)
	{
		if (mberr != 10)
			nresp = mb_common_err_resp(rbuf[1], mberr, g_mb_send_buf);
	}
	
FIN:
	if (nresp != 0)
		cont_write(fd, g_mb_send_buf, nresp);

	if (mberr == 10)
	{
		sleep_ms(1000);
		system_reset();
	}
}

