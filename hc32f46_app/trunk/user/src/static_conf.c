#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hc32f46_driver.h"
#include "customcmd.h"
#include "ModuleReader.h"

#define RDR_PARA_SG(pget) \
	do { \
		rerr = pget; \
		if (rerr != MT_OK_ERR) \
		{ \
			TRACE("%s err:%d\n", #pget, rerr); \
			return rerr; \
		} \
	} while (0)
	
extern volatile int gIsModAPICtrl;
extern volatile int gIsUnlockUart0;
extern volatile int isinitmodule;

READER_ERR getModlueParams(int hreader, ReaderStaticSettings_ST *pRsSettings)
{
	READER_ERR rerr = MT_OK_ERR;
	int tmpint;
	Region_Conf tmprg;
	HoptableData_ST *hptab1;
	HoptableData_ST *hptab2;
	int i;
	AntPowerConf pwrs;
	int savehtb = 0;
	
	if (isinitmodule == 1)
	{
		gIsModAPICtrl = 1;
		while(gIsUnlockUart0 == 0);
	}
	else
		return MT_INTERNAL_DEV_ERR;
	
	RDR_PARA_SG(ParamGet(hreader, MTR_PARAM_POTL_GEN2_SESSION, &tmpint));
	pRsSettings->protocol.gen2.session = tmpint;
	
	RDR_PARA_SG(ParamGet(hreader, MTR_PARAM_POTL_GEN2_Q, &tmpint));
	pRsSettings->protocol.gen2.q = tmpint;
	
	RDR_PARA_SG(ParamGet(hreader, MTR_PARAM_POTL_GEN2_TARGET, &tmpint));
	pRsSettings->protocol.gen2.target = tmpint;
	
	if (ParamGet(hreader, MTR_PARAM_POTL_GEN2_TAGENCODING, &tmpint) == MT_OK_ERR)
		pRsSettings->protocol.gen2.profile = tmpint;
	
	if (ParamGet(hreader, MTR_PARAM_RF_HOPANTTIME, &tmpint) == MT_OK_ERR)
		pRsSettings->rf.ant_max_dwell_time = tmpint;
	
	RDR_PARA_SG(ParamGet(hreader, MTR_PARAM_FREQUENCY_REGION, &tmprg));
	pRsSettings->rf.region = (int)tmprg;
	
	hptab1 = malloc_hexp(sizeof(HoptableData_ST));
	hptab2 = malloc_hexp(sizeof(HoptableData_ST));
	RDR_PARA_SG(ParamGet(hreader, MTR_PARAM_FREQUENCY_HOPTABLE, hptab1));
	RDR_PARA_SG(ParamSet(hreader, MTR_PARAM_FREQUENCY_REGION, &tmprg));
	RDR_PARA_SG(ParamGet(hreader, MTR_PARAM_FREQUENCY_HOPTABLE, hptab2));
	if (hptab1->lenhtb != hptab2->lenhtb)
		savehtb = 1;
	else
	{
		for (i = 0; i < hptab1->lenhtb; ++i)
		{
			if (hptab1->htb[i] != hptab2->htb[i])
			{
				savehtb = 1;
				break;
			}
		}
	}
	if (savehtb == 1)
	{
		pRsSettings->rf.hop_table_cnt = hptab1->lenhtb;
		for (i = 0; i < hptab1->lenhtb; ++i)
			pRsSettings->rf.hop_table[i] = hptab1->htb[i];
		RDR_PARA_SG(ParamSet(hreader, MTR_PARAM_FREQUENCY_HOPTABLE, hptab1));
	}
	free_hexp(hptab1);
	free_hexp(hptab2);
	RDR_PARA_SG(ParamGet(hreader, MTR_PARAM_RF_ANTPOWER, &pwrs));
	for (i = 0; i < pwrs.antcnt; ++i)
	{
		pRsSettings->rf.tx_powers[pwrs.Powers[i].antid-1].read_power = pwrs.Powers[i].readPower;
		pRsSettings->rf.tx_powers[pwrs.Powers[i].antid-1].write_power = pwrs.Powers[i].writePower;
	}
				
	RDR_PARA_SG(ParamGet(hreader, MTR_PARAM_TAGDATA_UNIQUEBYANT, &tmpint));
	pRsSettings->tag_data.unique_by_antenna = tmpint;
	
	RDR_PARA_SG(ParamGet(hreader, MTR_PARAM_TAGDATA_UNIQUEBYEMDDATA, &tmpint));
	pRsSettings->tag_data.unique_by_bank_data = tmpint;
	
	RDR_PARA_SG(ParamGet(hreader, MTR_PARAM_TAGDATA_RECORDHIGHESTRSSI, &tmpint));
	pRsSettings->tag_data.record_highest_rssi = tmpint;

	return rerr;
}
