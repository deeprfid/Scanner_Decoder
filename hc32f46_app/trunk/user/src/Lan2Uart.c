#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hc32f46_driver.h"
#include "customcmd.h"
#include "app_conf.h"
#include "mp_pool.h"
#include "reader_init.h"
#include "http_reader_api.h"

volatile int gIsModAPICtrl = 0;
volatile int gIsUnlockUart0 = 0;
volatile int porttype = 0;
volatile int uart0fd = COMMON_INTERFACE_UART0;
volatile int gCurInfFd = -1;

volatile int baudratenow = 115200;

volatile int isconnect = 0;
#define READ_UART0_N(buf, nr)       \
    ret = read_n(uart0fd, buf, nr); \
    if (ret == -1)                  \
        system_reset();             \
    else if (ret == -2)             \
        continue;

osMutexId_t buffermux;
osRtxMutex_t gBufmux_cb;
volatile int respbufwindex = 0;
// int mboxmsg = 0;
#define RESPBUF_LEN (1024 * 14)
#define MAXFRAMELEN 480
#define RESENDCNT 112
#define MBOXLEN 200
#define MAXSEMCOUNT 300
unsigned char *respbuffer;
volatile int respbufrindex = 0;

osSemaphoreId_t gBufDataSem;
osRtxSemaphore_t gBufDataSem_cb;
void dumpstate(void)
{

    TRACE("sem count:%d\n", osSemaphoreGetCount(gBufDataSem));
    TRACE("rindex:%d, windex:%d\n", respbufrindex, respbufwindex);
}

int gettcpinfo(int sock, int *sndbuf, int *sndquelen);
void dumptcp(int sock);

void sendthread(void *arg)
{

    int rc;
    int ret;
    int slen = 0;
    int tmpri;
    int tmpseg;
    int psendpos = 0;

    unsigned char sbuf[MAXFRAMELEN];

    while (1)
    {
        osSemaphoreAcquire(gBufDataSem, osWaitForever);
        osMutexAcquire(buffermux, osWaitForever);
        slen = respbufwindex - respbufrindex;

        if (slen == 0)
        {
            osMutexRelease(buffermux);
            continue;
        }

        if (slen > MAXFRAMELEN)
            slen = MAXFRAMELEN;

        tmpri = respbufrindex % RESPBUF_LEN;
        tmpseg = RESPBUF_LEN - tmpri;

        if (tmpseg >= slen)
            memcpy(sbuf, respbuffer + tmpri, slen);
        else
        {
            memcpy(sbuf, respbuffer + tmpri, tmpseg);
            memcpy(sbuf + tmpseg, respbuffer, slen - tmpseg);
        }

        osMutexRelease(buffermux);

        if (porttype == 1)
        {
            write(gCurInfFd, sbuf, slen);
            osMutexAcquire(buffermux, osWaitForever);
            respbufrindex += slen;
            osMutexRelease(buffermux);
        }
        else if (porttype == 2)
        {
            psendpos = 0;
            ret = 0;
SENDDATA:

            for (rc = 0; rc < RESENDCNT; ++rc)
            {
                if (isconnect == 1)
                {
                    ret = write(gCurInfFd, sbuf + psendpos, slen);

                    if (ret == slen)
                    {
                        osMutexAcquire(buffermux, osWaitForever);
                        respbufrindex += slen + psendpos;

                        if (respbufrindex > respbufwindex)
                        {
                            respbufrindex = 0;
                            respbufwindex = 0;
                        }

                        osMutexRelease(buffermux);
                        break;
                    }
                    else if (ret > 0)
                    {
                        TRACE("else if (ret > 0)n");
                        psendpos += ret;
                        slen -= ret;
                        goto SENDDATA;
                    }
                    else if (ret < 0)
                    {
                        osMutexAcquire(buffermux, osWaitForever);
                        respbufrindex = 0;
                        respbufwindex = 0;
                        osMutexRelease(buffermux);
                        break;
                    }
                }
                else
                {
                    osMutexAcquire(buffermux, osWaitForever);
                    respbufrindex = 0;
                    respbufwindex = 0;
                    osMutexRelease(buffermux);
                    break;
                }

                TRACE("write == 0\n");

                if (rc != RESENDCNT - 1)
                    sleep_ms(5);
            }
        }
    }
}

void sendforall(unsigned char *msgbuf, int msglen)
{
    int bufroom;
    int tmpwi;
    int tmpseg;

    osMutexAcquire(buffermux, osWaitForever);
    bufroom = RESPBUF_LEN - (respbufwindex - respbufrindex);

    if (bufroom < msglen)
    {
        osMutexRelease(buffermux);
        return;
    }
    else if (bufroom == RESPBUF_LEN)
    {
        respbufwindex = 0;
        respbufrindex = 0;
    }

    tmpwi = respbufwindex % RESPBUF_LEN;
    tmpseg = RESPBUF_LEN - tmpwi;

    if (tmpseg >= msglen)
        memcpy(respbuffer + tmpwi, msgbuf, msglen);
    else
    {
        memcpy(respbuffer + tmpwi, msgbuf, tmpseg);
        memcpy(respbuffer, msgbuf + tmpseg, msglen - tmpseg);
    }

    respbufwindex += msglen;
    osMutexRelease(buffermux);

    if (osSemaphoreGetCount(gBufDataSem) != MAXSEMCOUNT)
        osSemaphoreRelease(gBufDataSem);
}

int msgerrcnt = 0;
volatile int isinitmodule = 0;
volatile int isinituart1ex = 0;
void send_func(void *arg)
{
    int waitinitcnt = 0;
    int ret = 0;
    unsigned char recvbuf[255];
    int timeout1 = 10;
    int timeout2 = 20;
    #if Custom_By_SZBMA

    #else
    init_usb(gPRdrStaSet->app_init.usb_type);
    #endif

    if (get_uart_ex_dev() == Uart_Ex_Bluetooth)
        init_bluetooth(115200);

    isinituart1ex = 1;

    while (1)
    {
        if (isinitmodule == 1 ||
                gIsUnknownMod == 1)
            break;

        sleep_ms(50);
        waitinitcnt++;

        if (waitinitcnt >= 200)
            break;
    }

    if (gIsUnknownMod == 1)
        led_toggle(2000, 500, NULL);

    while (1)
    {
        if (gIsModAPICtrl == 1)
        {
            gIsUnlockUart0 = 1;
            sleep_ms(10);
            continue;
        }

        ioctl(uart0fd, COMMON_INTERFACE_SET_TIMEOUT, &timeout1);
        ret = read(uart0fd, recvbuf, 1);

        if (ret != 1)
            continue;

        if (recvbuf[0] != 0xff)
        {

            continue;
        }

        ioctl(uart0fd, COMMON_INTERFACE_SET_TIMEOUT, &timeout2);

        READ_UART0_N(recvbuf + 1, 4);

        if (recvbuf[1] > 248)
        {

            continue;
        }

        READ_UART0_N(recvbuf + 5, recvbuf[1] + 2);
        sendforall(recvbuf, recvbuf[1] + 7);
    }
}

#define READ_N_INFS(fd, buf, len)              \
    do                                         \
    {                                          \
        ret = read_n(fd, buf, len);            \
        if (fd <= COMMON_INTERFACE_SOCKET1)    \
        {                                      \
            if (ret <= 0)                      \
            {                                  \
                disconnect(gCurInfFd);         \
                close(gCurInfFd);              \
                TRACE("read_n err:%d\n", ret); \
                isconnect = 0;                 \
                continue;                      \
            }                                  \
        }                                      \
        else                                   \
        {                                      \
            if (ret == -1)                     \
                system_reset();                \
            else if (ret == -2)                \
                continue;                      \
        }                                      \
    } while (0)

void dumpcmd(unsigned char *buf, int dlen)
{
    #ifdef _DEBUG
    int i;

    for (i = 0; i < dlen; ++i)
        TRACE("%02X ", buf[i]);

    TRACE("\n");
    #endif
}

void Conn_Coming_Handler(void)
{
    osMutexAcquire(buffermux, osWaitForever);
    respbufrindex = 0;
    respbufwindex = 0;
    osMutexRelease(buffermux);
    isconnect = 1;
}

void Conn_Close_Handler(void)
{
    isconnect = 0;
}

extern ReaderStaticSettings_ST *gRsSetting;
int gRdrHandlePassive;
void modbus_func(int fd, uint8 *rbuf);
void init_modbus(void);
int sendforall_wrapper(int fd, uint8 *buf, int len)
{
    sendforall(buf, len);
    return 0;
}

int is_custom_cmd(int fd, uint8 *buf, int *nparse);
int is_modbus_potl(int fd, uint8 *buf, int nparse);
void active_http_post(void)
{
    int ret = 0;
    httpapi_init();

    if (ret == 0)
    {
        isinitmodule = 1;
    }
}

void user_main_passive(void)
{

    osThreadAttr_t thAttr_t;
    int reason = 0;
    int ret = 0;
    unsigned char recvbuf[255];

    int nParse;
    int m_rtm = 40;
    apt_pair_socks_st apt_st;
    apt_pair_socks_st *pApt_st;
    int uarts[3];
    int uartcnt = 0;
    commonUartPara uartPara;
    int Uarttimeout = 40;
    int Usbtimeout = 40;
    int UartExtimeout = 800;

    memset(&uartPara, 0, sizeof(uartPara));
    uartPara.baudrate = 115200;
    uartPara.timeout = -1;

    osMutexAttr_t mux_attr =
    {
        NULL,
        osMutexRecursive | osMutexPrioInherit,
        &gBufmux_cb,
        sizeof(gBufmux_cb)
    };
    osSemaphoreAttr_t sem_attr =
    {
        NULL,
        NULL,
        &gBufDataSem_cb,
        sizeof(gBufDataSem_cb)
    };

    memset(&apt_st, 0, sizeof(apt_st));
    apt_st.sns[0] = COMMON_INTERFACE_SOCKET0;
    apt_st.sns[1] = COMMON_INTERFACE_SOCKET1;
    apt_st.port = gNetConf.listenPort;
    apt_st.create_conn_cb = Conn_Coming_Handler;
    apt_st.close_conn_cb = Conn_Close_Handler;

    if (get_spi_ex_dev() == Spi_Ex_Ethernet)
        pApt_st = &apt_st;
    else
        pApt_st = NULL;

    buffermux = osMutexNew(&mux_attr);
    gBufDataSem = osSemaphoreNew(MAXSEMCOUNT, 0, &sem_attr);

    respbuffer = malloc_hexp(RESPBUF_LEN);

    init_osThreadAttr_t(&thAttr_t, 1024 * 2, osPriorityNormal);
    osThreadNew(send_func, NULL, &thAttr_t);

    init_osThreadAttr_t(&thAttr_t, 1024 * 2, osPriorityNormal);
    osThreadNew(sendthread, NULL, &thAttr_t);

    TRACE("before init_rfidmodle:%lld\n", getSysTick());
    httpapi_init();
    ret = httpapi_openrdr(&gRdrHandlePassive);

    TRACE("httpapi_init ret:%d\n", ret);

    if (ret == 0)
    {
        // init_modbus();
        int tagbufsize = get_left_heap_size("tagbufsize") - DynMemReserveSize;
        tagbufsize += 8 - tagbufsize % 8;
        TRACE("tagbufsize:%d\n", tagbufsize);
        httpapi_create_buffer(gPRdrStaSet->app_init.max_tb_rec_len, tagbufsize);
        get_left_heap_size("fin_pav_mode");
        isinitmodule = 1;

        if (gPRdrStaSet->uart1.baud < uart0_bauds[uart0_bindex])
            uartPara.baudrate = uart0_bauds[uart0_bindex];
        else
        {
            uartPara.baudrate = gPRdrStaSet->uart1.baud;
            uartPara.databits = gPRdrStaSet->uart1.data_bits;
            uartPara.stopbits = gPRdrStaSet->uart1.stop_bits;
            uartPara.parity = gPRdrStaSet->uart1.parity;
            uartPara.flowctrl = gPRdrStaSet->uart1.flow_ctrl;
        }

        if (get_uart_ex_dev() == Uart_Ex_Wlan)
        {
            if (uart0_bauds[uart0_bindex] > 115200)
                init_wlan(460800, NULL, 0, 0, 0);
            else
                init_wlan(115200, NULL, 0, 0, 0);
        }

        while (1)
        {
            if (isinituart1ex == 1)
                break;

            sleep_ms(50);
        }

        led_on();
        TRACE("after init_rfidmodle:%lld\n", getSysTick());
    }
    else
    {
        commonUartPara uart0Para;
        memset(&uart0Para, 0, sizeof(commonUartPara));
        uart_close(COMMON_INTERFACE_UART0);

        if (gIsUnknownMod == 1)
            uart0Para.baudrate = uart0_bauds[uart0_bindex];
        else
            uart0Para.baudrate = 115200;

        uart0Para.isRdam = 1;
        uart_open(COMMON_INTERFACE_UART0, &uart0Para);
        TRACE("init_rfidmodle failed:%lld\n", getSysTick());
    }

    #if (AppDubugPrintf != 1 || (!defined(_DEBUG)))

    if (get_uart_ex_dev() == Uart_Ex_None ||
            get_uart_ex_dev() == Uart_Ex_4G)
        uartPara.isRdam = 1;

    uart_open(COMMON_INTERFACE_UART2, &uartPara);
    ioctl(COMMON_INTERFACE_UART2, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
    ioctl(COMMON_INTERFACE_UART2, COMMON_INTERFACE_SET_TIMEOUT, &Uarttimeout);
    uarts[uartcnt++] = COMMON_INTERFACE_UART2;
    #endif
    //////
    memset(&uartPara, 0, sizeof(uartPara));
    uartPara.baudrate = gPRdrStaSet->uart2.baud;
    uartPara.databits = gPRdrStaSet->uart2.data_bits;
    uartPara.stopbits = gPRdrStaSet->uart2.stop_bits;
    uartPara.parity = gPRdrStaSet->uart2.parity;
    uartPara.flowctrl = gPRdrStaSet->uart2.flow_ctrl;
    uartPara.t485 = 1;
    uart_open(COMMON_INTERFACE_UART3, &uartPara);
    ioctl(COMMON_INTERFACE_UART3, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
    ioctl(COMMON_INTERFACE_UART3, COMMON_INTERFACE_SET_TIMEOUT, &Uarttimeout);
    uarts[uartcnt++] = COMMON_INTERFACE_UART3;
    //////

    if (get_uart_ex_dev() == Uart_Ex_Wlan || get_uart_ex_dev() == Uart_Ex_Bluetooth)
    {
        ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
        ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_SET_TIMEOUT, &UartExtimeout);
        uarts[uartcnt++] = COMMON_INTERFACE_UART1;
    }

    ioctl(COMMON_INTERFACE_USB0, COMMON_INTERFACE_SET_TIMEOUT, &Usbtimeout);
    ioctl(COMMON_INTERFACE_USB1, COMMON_INTERFACE_SET_TIMEOUT, &Usbtimeout);

    ioctl(COMMON_INTERFACE_SOCKET0, COMMON_INTERFACE_SET_TIMEOUT, &m_rtm);
    ioctl(COMMON_INTERFACE_SOCKET1, COMMON_INTERFACE_SET_TIMEOUT, &m_rtm);

    while (1)
    {
        gCurInfFd = apt_multi_infs_select(pApt_st, uarts, uartcnt, NULL, 0);

        if (gCurInfFd == -1)
        {
            TRACE("if (gCurInfFd == -1)\n");
            system_reset();
        }

        if (gCurInfFd <= COMMON_INTERFACE_SOCKET1)
            porttype = 2;
        else
            porttype = 1;

        READ_N_INFS(gCurInfFd, recvbuf, 3);
        nParse = 3;
        ret = is_custom_cmd(gCurInfFd, recvbuf, &nParse);

        if (ret == 1) // custom command
        {
            if ((recvbuf[0] == 'I') && (recvbuf[1] == 'P') && (recvbuf[2] == 'S'))
            {
                READ_N_INFS(gCurInfFd, recvbuf + 3, 22);
                custom_setip(recvbuf);
            }

            else if ((recvbuf[0] == 'I') && (recvbuf[1] == 'P') && (recvbuf[2] == 'G'))
            {
                READ_N_INFS(gCurInfFd, recvbuf + 3, 2);
                custom_getip(recvbuf);
            }
            else if ((recvbuf[0] == 'I') && (recvbuf[1] == 'O') && (recvbuf[2] == 'S'))
            {
                READ_N_INFS(gCurInfFd, recvbuf + 3, 3);
                READ_N_INFS(gCurInfFd, recvbuf + 6, recvbuf[5] * 2);
                custom_gposet(recvbuf);
            }
            else if ((recvbuf[0] == 'G') && (recvbuf[1] == 'I') && (recvbuf[2] == 'O'))
            {
                custom_gpiget2(recvbuf);
            }
            else if ((recvbuf[0] == 'S') && (recvbuf[1] == 'I') && (recvbuf[2] == 'O'))
            {
                READ_N_INFS(gCurInfFd, recvbuf + 3, 1);
                READ_N_INFS(gCurInfFd, recvbuf + 4, recvbuf[3] * 2);
                custom_gposet2(recvbuf);
            }
            else if ((recvbuf[0] == 'I') && (recvbuf[1] == 'O') && (recvbuf[2] == 'G'))
            {
                READ_N_INFS(gCurInfFd, recvbuf + 3, 2);
                custom_gpiget(recvbuf);
            }
            else if ((recvbuf[0] == 'I') && (recvbuf[1] == 'N') && (recvbuf[2] == 'I'))
            {
                READ_N_INFS(gCurInfFd, recvbuf + 3, 5);

                if ((recvbuf[3] == 'T') && (recvbuf[4] == 'U') && (recvbuf[5] == 'P') &&
                        (recvbuf[6] == 'F'))
                    firmware_upgrade(recvbuf[7]);
            }
            else if ((recvbuf[0] == 'p') && (recvbuf[1] == '1') && (recvbuf[2] == '1'))
            {
                system_reset();
            }
            else if ((recvbuf[0] == 'V') && (recvbuf[1] == 'E') && (recvbuf[2] == 'R'))
            {
                uint8 sendver[4];
                firmware_version(sendver);
                write(gCurInfFd, sendver, 4);
            }
            else if ((recvbuf[0] == 'S') && (recvbuf[1] == 'M') && (recvbuf[2] == '6'))
            {
                READ_N_INFS(gCurInfFd, recvbuf + 3, 1);
                custom_setm6ebaud230400(uart0fd, recvbuf);
                baudratenow = 230400;
            }
            else if ((recvbuf[0] == 'G') && (recvbuf[1] == 'M') && (recvbuf[2] == '6'))
            {
                unsigned char state = 0;
                READ_N_INFS(gCurInfFd, recvbuf + 3, 1);

                if (baudratenow == 230400)
                    state = 1;
                else
                    state = 0;

                write(gCurInfFd, &state, 1);
            }
            else if ((recvbuf[0] == 'C') && (recvbuf[1] == 'O') && (recvbuf[2] == 'N'))
            {
                READ_N_INFS(gCurInfFd, recvbuf + 3, 5);

                if (recvbuf[6] == 'R')
                {
                    READ_N_INFS(gCurInfFd, recvbuf + 8, 1);
                    custom_getconfig(recvbuf);
                }
                else if (recvbuf[6] == 'W')
                {
                    READ_N_INFS(gCurInfFd, recvbuf + 8, 201);
                    custom_setconfig(recvbuf);
                }
            }
            else if ((recvbuf[0] == 0xee) && (recvbuf[1] == 0x00))
            {
                READ_N_INFS(gCurInfFd, recvbuf + 3, 3);

                if (custom_ee_commond(gCurInfFd, recvbuf, 1, SaveCurStaticSettings,
                                      sendforall_wrapper, crc_Msg, NULL) != 0)
                    continue;
            }
            else if (((recvbuf[0] == 'P') && (recvbuf[1] == 'O') && (recvbuf[2] == 'S')) ||
                     ((recvbuf[0] == 'O') && (recvbuf[1] == 'P') && (recvbuf[2] == 'T')))
            {
                if (isinitmodule == 1)
                {
                    if (gCurInfFd <= COMMON_INTERFACE_SOCKET1 || gCurInfFd == COMMON_INTERFACE_UART1)
                    {
                        gIsModAPICtrl = 1;

                        while (gIsUnlockUart0 == 0)
                            ;

                        httpapi_hander(gCurInfFd, (char *)recvbuf);
                    }
                }
            }
            else
            {
                if (isinitmodule == 1)
                {
                    gIsModAPICtrl = 1;

                    while (gIsUnlockUart0 == 0)
                        ;

                    if (is_modbus_potl(gCurInfFd, recvbuf, nParse))
                        modbus_func(gCurInfFd, recvbuf);
                }
            }
        }
        else if (ret == 0)
        {
            gIsModAPICtrl = 0;
            gIsUnlockUart0 = 0;

            if (recvbuf[1] == 0xff)
            {
                READ_N_INFS(gCurInfFd, recvbuf + nParse, 32 - nParse);
                custom_resetmodule(uart0fd, recvbuf);
            }
            else
            {
                READ_N_INFS(gCurInfFd, recvbuf + nParse, recvbuf[1] + 5 - nParse);
                write(uart0fd, recvbuf, recvbuf[1] + 5);
            }
        }
    }

    led_off();

    while (1)
    {
        sleep_ms(1000);

        if (reason == 1)
        {
            //			printf("user_main create socket err\n");
        }
        else if (reason == 2)
        {
            //			printf("user_main bind err\n");
        }
        else if (reason == 3)
        {
            //			printf("user_main listen err\n");
        }
        else if (reason == 4)
        {
            //			printf("user_main select  err\n");
        }
        else if (reason == 5)
        {
            //			printf("user_main accept err\n");
        }
        else if (reason == 6)
        {
            //			printf("user_main setsockopt TCP_NODELAY err\n");
        }
        else if (reason == 7)
        {
            //			printf("user_main setsockopt SO_RCVTIMEO err\n");
        }
        else
        {
            //			printf("config_reader Unknown reason\n");
        }
    }
}
