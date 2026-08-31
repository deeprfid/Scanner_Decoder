/**
 * @file ota_http.h
 * @brief F460 HTTP OTA 通道（设备 HTTP 服务器收 POST 整包）
 */
#ifndef OTA_HTTP_H
#define OTA_HTTP_H

void http_handle_conn(int fd);

#endif /* OTA_HTTP_H */
