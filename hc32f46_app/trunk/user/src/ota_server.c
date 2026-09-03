/**
 * @file ota_server.c
 * @brief F460 OTA HTTP 服务器（独立监听线程）
 *
 * 独立于业务 httpServer（那个 2048B 缓冲装不下固件包）：
 * 监听 listenPort+1 端口，浏览器/上位机 POST 整包 .otapkg -> http_handle_conn 流式收包
 *
 * v1.1: 与 F4A0 ota_integration.c ota_dispatch_task 的 HTTP 分支逐行一致
 *       （http_cnt 节流计数、SOCKET2 SET_TIMEOUT、acttimeout=100、每轮 sleep_ms(1) 让出）。
 */
#include <string.h>
#include "hc32f46_driver.h"   /* gNetConf, COMMON_INTERFACE_SOCKET2, ioctl */
#include "socket.h"
#include "ota_http.h"
#include "ota_server.h"

/* 与 F4A0 ota_integration.c ota_dispatch_task 的 HTTP(SOCKET2) 分支逐字一致 */
static void ota_http_task(void *arg)
{
    uint64 lastacttm = 0;
    int http_cnt = 0;   /* F4A0 v1.0: poll HTTP every 10 loops */
    int hrtm = 5000;
    int fd;
    (void)arg;

    TRACE("[ota] http server task started\n");

    /* HTTP OTA 监听口 = listenPort+1（与 F4A0 ota_dispatch_task 一致） */
    ioctl(COMMON_INTERFACE_SOCKET2, COMMON_INTERFACE_SET_TIMEOUT, &hrtm);

    for (;;) {
        /* F4A0 v1.0: HTTP select 节流——http_cnt>=10 才轮询 SOCKET2 */
        if (++http_cnt >= 10) {
            fd = apt_single_select_nob(COMMON_INTERFACE_SOCKET2,   /* 非阻塞，不阻塞本任务 */
                                       (unsigned short)(gNetConf.listenPort + 1),
                                       &lastacttm, 100);
            /* fd>=0=收到连接 / -1=监听失败 / -2=无活动 */
            if (fd >= 0) {
                TRACE("[ota] conn fd=%d port=%u\n", fd,
                      (unsigned)(gNetConf.listenPort + 1));
                http_handle_conn(fd);   /* 收 POST 整包 -> 写 QSPI -> 验签 -> mark_ready -> reset */
                disconnect(fd);
                close(fd);
            }
        }
        /* 真让出：sleep_ms(1)=osDelay(0) 不阻塞（5ms tick 下整除为 0），
         * 导致空闲忙转抢占业务；改 osDelay(1)=硬阻塞 1 tick(5ms)，
         * OTA 线程空闲时真正休眠，W5100S 只剩业务单方活动 */
        osDelay(1);
    }
}

void ota_server_start(void)
{
    osThreadAttr_t thAttr_t;

    /* 调度逻辑与 F4A0 ota_dispatch_start 一致（16KB 栈）；
     * 优先级用 Normal：F460 实测 High 会饿死业务初始化线程（user_main 卡在
     * "run user_main_passive" 之前），F4A0 无此现象——保持 Normal 即可，
     * ota 线程每轮 sleep_ms(1) 让出，不影响 HTTP OTA 吞吐。 */
    init_osThreadAttr_t(&thAttr_t, 1024 * 16, osPriorityNormal);
    osThreadNew(ota_http_task, NULL, &thAttr_t);
}
