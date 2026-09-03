# 修改记录 - HC32F4A020260320_RYK 扫描盘读写器（F460）

## 2026-09-04 OTA 独立线程干扰业务 socket 的根因与修复（真让出 osDelay(1)）

### 问题（用户实测）
- 开启 HTTP OTA 独立监听线程（SOCKET2:8081）后，上位机无法设置/获取参数：
  - 被动模式（业务 EEcmd 走 Lan2Uart 主循环 S0/S1）：应答写 s=0 sr=28（CLOSE_WAIT）；
  - 主动模式（业务 EEcmd 走 firmware_upgrade_process S1:8080）：应答写 s=1 sr=0（CLOSED）；
- 设备端 EEcmd 处理本身仅约 50-70ms（探针实测），但应答发出时连接已被上位机超时关闭；
- 关闭 OTA 线程后一切正常（对照实验）。

### 排查过程（8 个变体全部失败后定位）
| 变体 | 结果 |
|---|---|
| 1ms 轮询 / F4A0 verbatim / High 16KB / Normal 16KB / 8KB / 节流 | 业务 socket 仍死 |
| 线程存在但完全不碰 SOCKET2（探测版） | 业务正常（唯一正常形态） |
| W5100S SPI burst 批量收发提速（port.c 注册 burst） | 无效 |
- 逐函数对比 F460/F4A0（io_stream/socket.c/w5100s.c/wizchip_conf/port.c/初始化）：
  功能无差异；F4A0 CHANGELOG + git 历史（v9.81c send_func 竞争 USB1、03effaf 收拢
  ota_dispatch_task）显示 F4A0 模式是"通道归属+让路"，其 dispatch 有 USB 阻塞读作让出点。

### 根本原因
- OTA 线程空闲循环用 sleep_ms(1) "让出"——假让出：
  F460 RTX tick = 5ms（OS_TICK_FREQ 200，SYSTEM_TICK_DUR 5），sleep_ms(1) =
  osDelay(1/5=0) = osDelay(0) 不阻塞 → 线程空闲时纯忙转，持续抢占 W5100S 访问，
  干扰业务 socket 收发时序（F4A0 tick=2ms 且 dispatch 有 USB 阻塞读，故未暴露）。

### 修复（1 行）
- hc32f46_app/trunk/user/src/ota_server.c：sleep_ms(1) → osDelay(1)
  （RTX 硬阻塞 1 tick = 5ms，空闲真正让出；W5100S 只剩业务单方活动）。
- 真机验证：主动/被动模式 获取/设置参数恢复正常；8081 HTTP OTA 上传正常。

### 附：本批清理与保留
- 清理：Lan2Uart.c [dbg] 探针、driver custom_ee_commond.c [p] 探针、
  io_stream.c write 错误打印恢复 F4A0 原文；临时日志/文件删除。
- 保留：ota_server.c osDelay(1) 修复；port.c 注册 burst 批量收发
  （SPI_ReadDatas/SPI_WriteDatas，w5100s.c 自动走 burst，缩短 W5100S 传输占用）；
  driver common.c getMaxSocketId F4A0 对齐 + fw-upgrade 固定 SOCKET1。

### 平台计时差异备忘（F460 vs F4A0）
- RTX tick：F460 5ms（OS_TICK_FREQ 200）/ F4A0 2ms（500）；SYSTEM_TICK_DUR 5/2；
- 轮转时间片：F460 5ms（OS_ROBIN_TIMEOUT 1）/ F4A0 10ms（5）；
- 系统主频：app 侧 168MHz（MPLL 8M/1x42/2，boot 为 200MHz 8M/1x50/2）；
  W5100S SPI = PCLK1(84MHz)/4 = 21MHz（Div2=42MHz 超 W5100S 33.3MHz 上限，未提）。
- 教训：F460 上所有"每轮 sleep_ms(1) 让出"的独立线程都是忙转，需用 osDelay(>=1) 真阻塞。
## 2026-09-04 主频 168MHz -> 200MHz（driver 库，时钟/定时器按比例修改）

### 改动（hc32f46_driver/trunk）
- common.c / sysinit.c ClkInit：MPLL plln 42->50（8M/1x50/2=200M）+ 启用 PWC_HS2HP()（>150M 高性能模式，同 boot）
- timer.h：ONEMSC 82->98、ONEUSC 21->25（PCLK1 84M->100M）
- timer.c：getSysTick tcount/80->/98；TMRA4 512ms 周期 42000->50000
- 修复 timer_Delay_ms 16 位计数溢出死循环：原按 3ms 分块等 3000xONEUSC，ONEUSC=25 时 75000>65535 永不退出（曾卡在 W5100S Reset_W5100S 的 100ms/1000ms 延时，表现 app 跳转后无输出）；改按 1ms 分块（1000xONEUSC=25000 安全）
- wizchip/port.c：W5100S SPI Div2（PCLK1 100M/2=50MHz，用户实测 W5100S 支持至 70M）
- QSPI 时钟统一 HCLK/2=100MHz（app w25qxx.c QspiHclkDiv2 不变；boot boot_qspi.c QSPI_CLK_DIV3->DIV2，boot 与 app 一致；W25Q64 上限内）

### 验证
- 200MHz 全链路：boot 跳转 / 被动+主动业务 / HTTP OTA 均正常
- 期间"上位机不通/HTTP OTA 不通"经查为全片擦除导致 IP/配置丢默认，与主频无关，重存配置即恢复
- 附带收益：SystemCoreClock(200M) 与实际主频一致；UART 波特率运行时按 PCLK 计算自动对齐；W5100S SPI 21->25MHz、QSPI HCLK/2 84->100MHz