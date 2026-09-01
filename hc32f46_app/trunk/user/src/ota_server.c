/**
 * @file ota_server.c
 * @brief F460 OTA HTTP 服务器（独立监听线程）
 *
 * 独立于业务 httpServer（那个 2048B 缓冲装不下固件包）：
 * 监听 listenPort+1 端口，浏览器/上位机 POST 整包 .otapkg -> http_handle_conn 流式收包
 */
#include <string.h>
#include "hc32f46_driver.h"   /* gNetConf, COMMON_INTERFACE_SOCKET2 */
#include "socket.h"
#include "ota_http.h"
#include "ota_server.h"

static void ota_http_task(void *arg)
{
    uint64 lastacttm = 0;
    int fd;
    (void)arg;

    TRACE("[ota] http server task started\n");

    for (;;) {
        /* 非阻塞 select：无连接不阻塞（让出 CPU 给业务线程） */
        fd = apt_single_select_nob(COMMON_INTERFACE_SOCKET2,
                                   (unsigned short)(gNetConf.listenPort + 1),
                                   &lastacttm, 100);
        if (fd >= 0) {
            TRACE("[ota] conn fd=%d port=%u\n", fd,
                  (unsigned)(gNetConf.listenPort + 1));
            http_handle_conn(fd);   /* 收 POST 整包 -> 写 QSPI -> 验签 -> mark_ready -> reset */
            disconnect(fd);
            close(fd);
        }
        sleep_ms(1);
    }
}

void ota_server_start(void)
{
    osThreadAttr_t thAttr_t;
    /* Normal 优先级：避免抢占业务初始化线程（此前 High 会饿死业务初始化） */
    init_osThreadAttr_t(&thAttr_t, 1024 * 8, osPriorityNormal);
    osThreadNew(ota_http_task, NULL, &thAttr_t);
}
