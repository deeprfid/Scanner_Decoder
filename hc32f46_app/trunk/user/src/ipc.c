#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "ipc.h"
#include "hc32f46_driver.h"

unsigned short ipcCrc(unsigned char *msgbuf, int msglen);
void W25QXX_Init(void);
void set_eastag_to_flash(void);
osMutexId_t tb_SpiMux;
osRtxMutex_t gTb_SpiMux_cb;
__align(64) rfidcfg mycfgdata;
extern uint8_t tagtmpbuf[0x1000];
osMessageQueueId_t gIdMsgQue_alarm_tag;
osMessageQueueId_t gIdMsgQue_HPM6340;

void set_eas_defualt(void)
{
    uint16_t crc;
    crc = ipcCrc((unsigned char *)&mycfgdata, sizeof(mycfgdata) - 4);

    if(crc == mycfgdata.crc)
    {
        mycfgdata.totaltagcnt = 0;
        mycfgdata.totalalarmcnt = 0;
        return;
    }
    else
    {
        memset(&mycfgdata, 0, sizeof(mycfgdata));
        mycfgdata.tagstoragedays[0] = 0x35;
        mycfgdata.system_time[0] = 0x30;
        mycfgdata.radar_range = 5;
        mycfgdata.tag_read_cnt = 5;
        mycfgdata.alarm_volume = 5;
        mycfgdata.alarm_duration = 5;
			  mycfgdata.easflag = 0;
        set_eastag_to_flash();
    }
}

void httpbuf_init(void)
{
    osMessageQueueAttr_t mqAttr;
    int mqdsize;
    ////
    mqdsize = sizeof(MsgQueObj_alarm_tag);
    mqdsize += 32 - mqdsize % 32;
    mqdsize = mqdsize * 32;
    TRACE("mqdsize:%d\n", mqdsize);

    mqAttr.name = NULL;
    mqAttr.attr_bits = 0;
    mqAttr.cb_mem = malloc_hexp(sizeof(osRtxMessageQueue_t));
    mqAttr.cb_size = sizeof(osRtxMessageQueue_t);
    mqAttr.mq_mem = malloc_hexp(mqdsize);
    mqAttr.mq_size = mqdsize;

    gIdMsgQue_alarm_tag = osMessageQueueNew(4, sizeof(MsgQueObj_alarm_tag), &mqAttr);

    if (gIdMsgQue_alarm_tag != NULL)
        TRACE("osMessageQueueNew is ok\n");
    else
        TRACE("osMessageQueueNew is failed\n");

    memset(tagtmpbuf, 0, sizeof(tagtmpbuf));
}

int get_hpm6340_tag_que(MsgQueObj_HPM6340 *mqObj)
{
    osStatus_t gmqret;

    gmqret = osMessageQueueGet(gIdMsgQue_HPM6340, mqObj, NULL, NULL);

    if (gmqret == osOK)
    {
        return 0;
    }

    return -1;
}

extern int ghReader;

#include <stdint.h>
#include <stdbool.h>

uint8_t EASbitmatch(uint8_t *epcid, uint16_t tlen, uint32_t easflag, bool match_head_tail)
{
    if (easflag == 0 || epcid == NULL || tlen < 4)
        return 0;

    uint32_t tagbits;

    if (match_head_tail)
    {
        tagbits = (epcid[0] << 24) | (epcid[1] << 16) | (epcid[2] << 8) | epcid[3];
    }
    else
    {
        tagbits = (epcid[tlen - 4] << 24) |
                  (epcid[tlen - 3] << 16) |
                  (epcid[tlen - 2] << 8)  |
                  (epcid[tlen - 1]);
    }


    int highest = -1;

    for (int i = 7; i >= 0; --i)
    {
        if (((easflag >> (i * 4)) & 0xF) != 0)
        {
            highest = i;
            break;
        }
    }

    if (highest < 0) return 0;

    int k = highest + 1;

    if (match_head_tail)
    {

        for (int i = 0; i < k; ++i)
        {
            uint8_t tag_nib = (tagbits >> ((7 - i) * 4)) & 0xF;
            uint8_t eas_nib = (easflag >> ((k - 1 - i) * 4)) & 0xF;

            if (tag_nib != eas_nib) return 0;
        }
    }
    else
    {

        for (int i = 0; i < k; ++i)
        {
            uint8_t tag_nib = (tagbits >> (i * 4)) & 0xF;
            uint8_t eas_nib = (easflag >> (i * 4)) & 0xF;

            if (tag_nib != eas_nib) return 0;
        }
    }

    return 1;
}




void SetEasBit(uint8_t *epcid, uint16_t tlen, const uint32_t easflag, bool match_head_tail)
{
    if (epcid == NULL || tlen < 4)
        return;

    if (easflag == 0)
        return;

    if (match_head_tail)
    {

        uint8_t n[8];
        n[0] = (epcid[0] >> 4) & 0xF;
        n[1] = epcid[0] & 0xF;
        n[2] = (epcid[1] >> 4) & 0xF;
        n[3] = epcid[1] & 0xF;
        n[4] = (epcid[2] >> 4) & 0xF;
        n[5] = epcid[2] & 0xF;
        n[6] = (epcid[3] >> 4) & 0xF;
        n[7] = epcid[3] & 0xF;


        int k = 0;

        int highest_nibble = -1;

        for (int i = 7; i >= 0; --i)
        {
            if (((easflag >> (i * 4)) & 0xF) != 0)
            {
                highest_nibble = i;
                break;
            }
        }

        if (highest_nibble < 0) return;


        k = highest_nibble + 1;


        uint8_t eas_nibbles[8];

        for (int i = 0; i < k; ++i)
        {

            int shift = (k - 1 - i) * 4;
            eas_nibbles[i] = (easflag >> shift) & 0xF;
        }


        for (int i = 0; i < k; ++i)
            n[i] = eas_nibbles[i];


        epcid[0] = (uint8_t)((n[0] << 4) | (n[1] & 0xF));
        epcid[1] = (uint8_t)((n[2] << 4) | (n[3] & 0xF));
        epcid[2] = (uint8_t)((n[4] << 4) | (n[5] & 0xF));
        epcid[3] = (uint8_t)((n[6] << 4) | (n[7] & 0xF));
    }
    else
    {

        uint32_t tag = (epcid[tlen - 4] << 24) | (epcid[tlen - 3] << 16) |
                       (epcid[tlen - 2] << 8)  | epcid[tlen - 1];


        int valid_nibbles = 8;

        while (valid_nibbles > 0 &&
                ((easflag >> ((valid_nibbles - 1) * 4)) & 0xF) == 0)
            valid_nibbles--;

        if (valid_nibbles == 0)
            return;

        uint32_t mask = 0;

        for (int i = 0; i < valid_nibbles; i++)
            mask |= (0xFu << (i * 4));

        tag &= ~mask;
        tag |= (easflag & mask);

        epcid[tlen - 4] = (tag >> 24) & 0xFF;
        epcid[tlen - 3] = (tag >> 16) & 0xFF;
        epcid[tlen - 2] = (tag >> 8)  & 0xFF;
        epcid[tlen - 1] = tag & 0xFF;
    }
}


unsigned int swapEndian(unsigned int num)
{
    return ((num >> 24) & 0x000000FF) |
           ((num >> 8) & 0x0000FF00) |
           ((num << 8) & 0x00FF0000) |
           ((num << 24) & 0xFF000000);
}

void MessageQueueReset(osMessageQueueId_t mq_id)
{
    osMessageQueueReset(mq_id);
}

void Tag_update_thread(void *arg)
{
    MsgQueObj_HPM6340 hpmtag;
    unsigned char acpwd[9] = {0};
    extern uint8_t Keyvalue;
    static uint8_t lastkey = 0;
    static uint8_t rollback_flag = 0;

    while (1)
    {


        if (osMessageQueueGetCount(gIdMsgQue_HPM6340))
        {
            uint8_t EasBit = 0;
            osStatus_t gmqret;
            memset(&hpmtag, 0, sizeof(hpmtag));

            if (mycfgdata.easflag && rollback_flag == 0)
            {
                gmqret = osMessageQueueGet(gIdMsgQue_HPM6340, &hpmtag, NULL, NULL);

                if (gmqret == osOK)
                {
                    EasBit = EASbitmatch(hpmtag.epcid, hpmtag.epclen, mycfgdata.easflag, MATCHTAIL);
                }

                if (EasBit == 0)
                {
                    SetEasBit(hpmtag.epcid, hpmtag.epclen, mycfgdata.easflag, MATCHTAIL);
                    osMutexAcquire(tb_SpiMux, osWaitForever);

                    do
                    {

                    }
                    while (MT_OK_ERR != WriteTagEpcEx(ghReader, 1, hpmtag.epcid, hpmtag.epclen, acpwd, 100));

                    osMutexRelease(tb_SpiMux);
                    MessageQueueReset(gIdMsgQue_HPM6340);
                }

                BLUE_LED_OFF();
                beep_on();
                GREEN_LED_ON();
                osDelay(15);
                beep_off();
                GREEN_LED_OFF();
                osDelay(15);
            }

            if (mycfgdata.easflag == 0 && rollback_flag == 0)
            {
                beep_on();
                BLUE_LED_OFF();
                GREEN_LED_ON();
                UDP_SendAll(gIdMsgQue_HPM6340, OPTION_ADD);
                osDelay(20);
                beep_off();
                GREEN_LED_OFF();
                osDelay(20);
            }

            if (mycfgdata.easflag == 0 && rollback_flag == 1)
            {
                BLUE_LED_OFF();
                beep_on();
                RED_LED_ON();
                UDP_SendAll(gIdMsgQue_HPM6340, OPTION_DEL);
                osDelay(20);
                beep_off();
                RED_LED_OFF();
                osDelay(20);
            }

            if (mycfgdata.easflag && rollback_flag == 1)
            {
                memset(&hpmtag, 0, sizeof(hpmtag));
                gmqret = osMessageQueueGet(gIdMsgQue_HPM6340, &hpmtag, NULL, NULL);

                if (gmqret == osOK)
                {
                    RollBack(&hpmtag);
                }
            }

        }

        if (lastkey ^ Keyvalue)
        {
            lastkey = Keyvalue;

            if (lastkey)
            {
                rollback_flag = 1;
            }
            else
            {
                rollback_flag = 0;
            }
        }

        LED_Runing_Status(1);
        // osThreadYield();
    }
}

void LED_Runing_Status(uint8_t startbit)
{
    static unsigned long long Ticknow = 0;
    Ticknow = getSysTick();

    if (Ticknow % 3200 == 0 && startbit)
    {
        BLUE_LED_ON();
        return;
    }

    if (Ticknow % 400 == 0 && startbit)
    {
        BLUE_LED_OFF();
        return;
    }
}

void UDP_SendAll(osMessageQueueId_t sendmsg, uint8_t optioncode)
{
    uint8_t que_cnt = 0;
    osStatus_t gmqret;
    MsgQueObj_HPM6340 mqObj;

    que_cnt = osMessageQueueGetCount(sendmsg);

    for (uint8_t i = 0; i < que_cnt; i++)
    {
        gmqret = osMessageQueueGet(sendmsg, &mqObj, NULL, NULL);

        if (gmqret == osOK)
        {
            UDP_upload(&mqObj, optioncode);
            osDelay(1);
        }
    }
}

void UDP_upload(MsgQueObj_HPM6340 *udpepc, uint8_t optioncode)
{

    udp_package brdcstudp;
    uint8_t macaddr[6] = {0};
    brdcstudp.frameHead = 0xEE;
    brdcstudp.maclen = 0x06;
    brdcstudp.cmdflag = optioncode;
    brdcstudp.cmdcode = 0x54;
    SetNumU16((uint8_t *)&brdcstudp.datalen, udpepc->epclen);
    brdcstudp.epclen = udpepc->epclen;
    memcpy(brdcstudp.macaddr, macaddr, 0x06);
    memcpy(brdcstudp.epc, udpepc->epcid, udpepc->epclen);
    uint16_t crc = ipcCrc((unsigned char *)&brdcstudp, sizeof(brdcstudp) - 2);
    brdcstudp.crc = crc;
    UDP_send((uint8_t *)&brdcstudp, sizeof(brdcstudp));
}

void RollBack(MsgQueObj_HPM6340 *RollBackTag)
{
    unsigned char acpwd[9] = {0};
    MsgQueObj_HPM6340 tagTID;
    memset(&tagTID, 0, sizeof(tagTID));

    if (RollBackTag->EmbededDatalen > 0 && RollBackTag->EmbededDatalen <= 32)
    {
        tagTID.epclen = RollBackTag->EmbededDatalen;
        memcpy(tagTID.epcid, RollBackTag->EmbededData, RollBackTag->EmbededDatalen);
    }

    if (memcmp(RollBackTag->epcid, RollBackTag->EmbededData, RollBackTag->EmbededDatalen) == 0)
    {
        goto FINEXIT;
    }
    else
    {
        osMutexAcquire(tb_SpiMux, osWaitForever);

        while (MT_OK_ERR != WriteTagEpcEx(ghReader, 1, tagTID.epcid, tagTID.epclen, acpwd, 100))
        {
        }

        osMutexRelease(tb_SpiMux);
        MessageQueueReset(gIdMsgQue_HPM6340);
    }

FINEXIT: // 2025-02-14
    BLUE_LED_OFF();
    beep_on();
    RED_LED_ON();
    osDelay(15);
    beep_off();
    RED_LED_OFF();
    osDelay(15);
}

void Killepctag(MsgQueObj_HPM6340 *RollBackTag)
{
    // int bank = 0;
    // int addr = 2;
    // unsigned char    acpwd[9]={0};
    // unsigned char kpwd[4];

    // kpwd[0] = 0x88;
    // kpwd[1] = 0x88;
    // kpwd[2] = 0x88;
    // kpwd[3] = 0x88;

    //    osMutexAcquire(tb_SpiMux, osWaitForever);
    //    if (WriteTagData(ghReader, 1, bank, addr, kpwd, 4, acpwd, 100) != MT_OK_ERR)
    //    {
    //       printf("WriteTagData failed\n");
    //			 goto KILLEXIT;
    //    }

    //		if (KillTag(ghReader, 1, kpwd, 100) != MT_OK_ERR)
    //		{
    //			 printf("KillTag failed \n");
    //			 goto KILLEXIT;
    //		}
    //
    //    BLUE_LED_OFF();
    //	  beep_on();
    //	  RED_LED_ON();
    //		osDelay(15);
    //		beep_off();
    //		RED_LED_OFF();
    //	  osDelay(15);
    // KILLEXIT:
    //    osMutexRelease(tb_SpiMux);
    //	  MessageQueueReset(gIdMsgQue_alarm_tag);
    //		MessageQueueReset(gIdMsgQue_HPM6340);
}

void LED_test(void)
{
    BLUE_LED_OFF();
    RED_LED_OFF();
    GREEN_LED_OFF();

    beep_on();
    RED_LED_ON();
    osDelay(50);
    RED_LED_OFF();
    osDelay(50);

    BLUE_LED_ON();
    osDelay(50);
    BLUE_LED_OFF();
    osDelay(50);

    GREEN_LED_ON();
    osDelay(50);
    GREEN_LED_OFF();
    beep_off();
}

void HPM6340msg_init(void)
{
    osThreadAttr_t thAttr_t;
    osMessageQueueAttr_t mqAttr;
    int mqdsize;
    ////
    mqdsize = sizeof(MsgQueObj_HPM6340);
    mqdsize += 32 - mqdsize % 32;
    mqdsize = mqdsize * 32;
    TRACE("mqdsize:%d\n", mqdsize);

    mqAttr.name = NULL;
    mqAttr.attr_bits = 0;
    mqAttr.cb_mem = malloc_hexp(sizeof(osRtxMessageQueue_t));
    mqAttr.cb_size = sizeof(osRtxMessageQueue_t);
    mqAttr.mq_mem = malloc_hexp(mqdsize);
    mqAttr.mq_size = mqdsize;

    gIdMsgQue_HPM6340 = osMessageQueueNew(16, sizeof(MsgQueObj_HPM6340), &mqAttr);

    if (gIdMsgQue_HPM6340 != NULL)
        TRACE("osMessageQueueNew is ok\n");
    else
        TRACE("osMessageQueueNew is failed\n");

    init_osThreadAttr_t(&thAttr_t, 1024 * 2, osPriorityNormal);
    osThreadNew(Tag_update_thread, NULL, &thAttr_t);
}

int8_t filte_rule_process(uint8_t *epcid, uint8_t epclen)
{
    uint8_t mask[20] = {0};
    uint8_t result = 0xff, flag_mask = 0, match_mask = 0;

    for (uint8_t i = 0; i < 10; i++)
    {
        if (mycfgdata.tagfilter_rule[i].chstate == 0)
        {
            flag_mask++;
            continue;
        }

        if (mycfgdata.tagfilter_rule[i].chstate == 1)
        {
            strTohex(mycfgdata.tagfilter_rule[i].maskcode, mycfgdata.tagfilter_rule[i].match_len, mask);
            result = memcmp(epcid + mycfgdata.tagfilter_rule[i].start_addr, mask, (mycfgdata.tagfilter_rule[i].match_len) / 2);

            if (result == 0)
            {
                match_mask++;
            }
        }
    }

    if (flag_mask == 10)
    {
        return 0x1;
    }
    else if (flag_mask < 10 && match_mask)
    {
        return 0x1;
    }
    else if (flag_mask < 10 && match_mask == 0)
    {
        return 0;
    }

    return -2;
}

int put_tag_que(TAGINFO *tag)
{
    static uint8_t EPCID[EPCIDMAXLEN] = {0};
    osStatus_t osSta;
    MsgQueObj_alarm_tag mytagobj;
    MsgQueObj_HPM6340   hpmtag;
    // uint8_t que_cnt=0;

    if (0x00 == filte_rule_process(tag->EpcId, tag->Epclen))
    {
        return -2;
    }

    hpmtag.ant = tag->AntennaID;
    hpmtag.frameHead = 0xFF;
    hpmtag.epclen = tag->Epclen;
    memcpy(&hpmtag.epcid, tag->EpcId, tag->Epclen);
    memcpy(&hpmtag.PC, tag->PC, 2);
    memcpy(&hpmtag.EmbededData, tag->EmbededData, tag->EmbededDatalen);
    hpmtag.EmbededDatalen = tag->EmbededDatalen;
    osSta = osMessageQueuePut(gIdMsgQue_HPM6340, &hpmtag, NULL, NULL);

    // que_cnt=osMessageQueueGetCount(gIdMsgQue_alarm_tag);

    // if(memcmp(tag->EpcId,EPCID,tag->Epclen) || que_cnt==0)
    {
        memset(&mytagobj, 0, sizeof(mytagobj));
        mytagobj.ant = tag->AntennaID;
        memcpy(EPCID, tag->EpcId, tag->Epclen);
        mytagobj.epclen = tag->Epclen;
        memcpy(&mytagobj.epcid, tag->EpcId, tag->Epclen);
        memcpy(&mytagobj.PC, tag->PC, 2);
        osSta = osMessageQueuePut(gIdMsgQue_alarm_tag, &mytagobj, NULL, NULL);

        // osSta = osMessageQueuePut(gIdMsgQue_HPM6340  , &hpmtag  , NULL, NULL);
        if (osSta != osOK)
            return -2;
    }

    return 0;
}

uint8_t get_tag_counter(void)
{
    uint8_t que_cnt = 0;

    que_cnt = osMessageQueueGetCount(gIdMsgQue_alarm_tag);

    return que_cnt;
}

int get_tag_que(MsgQueObj_alarm_tag *mqObj)
{
    osStatus_t gmqret;
    gmqret = osMessageQueueGet(gIdMsgQue_alarm_tag, mqObj, NULL, NULL);

    if (gmqret == osOK)
    {
        return 0;
    }

    return -1;
}

uint8_t removeDuplicates(uint8_t *arr, uint8_t *epcid, uint8_t len)
{

    uint8_t j = 0;

    for (int i = 0; i < QUELENTH * 5; i++)
    {
        if (memcmp(arr + i * EPCIDMAXLEN, epcid, len) == 0)
        {

            j++;
        }
    }

    return j ? 0 : -1;
}

void tagfiltbuff_init(void)
{

    get_eastag_to_flash();
    set_eas_defualt();
    httpbuf_init();
    HPM6340msg_init();
//    W25QXX_Init();
    osMutexAttr_t mux_attr =
    {
        NULL,
        osMutexRecursive | osMutexPrioInherit,
        &gTb_SpiMux_cb,
        sizeof(gTb_SpiMux_cb)
    };
    tb_SpiMux = osMutexNew(&mux_attr);
}

void deviceID_update(ReaderRunTimeSettings_ST *prtset)
{
    int namelen = strlen(prtset->glob_params.name);

    if (namelen > 0 && namelen < 20)
    {
        memset(&mycfgdata.deviceID, 0, sizeof(mycfgdata.deviceID));
        memcpy(&mycfgdata.deviceID, prtset->glob_params.name, namelen);
    }
}

void rd_idkey_fun(void)
{

    static uint32_t uuid;
    uuid = Ucode_read((uint8_t *)&uuid, sizeof(uuid));

    if (uuid != 0x42BC6388)
    {
        for(uint8_t i = 0; i < 3; i++)
        {
            beep_on();
            RED_LED_ON();
            osDelay(20);
            beep_off();
            RED_LED_OFF();
            osDelay(20);
        }

				led_toggle(-1, 100, NULL);
        while (uuid)
        {           
        }
    }
    else
    {
        for(uint8_t i = 0; i < 2; i++)
        {
            beep_on();
            RED_LED_ON();
            osDelay(40);
            beep_off();
            RED_LED_OFF();
            osDelay(40);
        }
    }
}

void set_eastag_to_flash(void)
{

    uint8_t *buf_ = (uint8_t *)&mycfgdata;
    uint16_t crc;
    crc = ipcCrc((unsigned char *)&mycfgdata, sizeof(mycfgdata) - 4);
    mycfgdata.crc = crc;
    flash_sector_erase(EastagPage_Addr);
    flash_bytes_write(EastagPage_Addr, buf_, sizeof(mycfgdata));
}

void Erase_eastag_to_flash(void)
{

    uint8_t *buf_ = (uint8_t *)&mycfgdata;
    memset(&mycfgdata, 0, sizeof(mycfgdata));
    flash_sector_erase(EastagPage_Addr);
    flash_bytes_write(EastagPage_Addr, buf_, sizeof(mycfgdata));
}

void get_eastag_to_flash(void)
{

    flash_bytes_read(EastagPage_Addr, &mycfgdata, sizeof(mycfgdata));
}
