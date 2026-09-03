/**
 *******************************************************************************
 * @file  boot_qspi.c
 * @brief F460 Boot QSPI (W25Q64 8MB) 精简驱动�? *
 *   引脚（F460 JEUA QFN48，与 App w25qxx.c 一致）�? *     QSCK=PB14  QSSN=PB01  QSIO0=PB13  QSIO1=PB12  QSIO2=PB10  QSIO3=PB02
 *     GPIO_FUNC_7，XIP 窗口 0x98000000�? *   读：内存映射(XIP) QUAD IO Fast Read�?4 位地址（W25Q64）�? *   注：W25Q64JV 状态寄存器为易失（上电 QE=0），boot 每次启动自行�?QE�? *       否则 QUAD 读无效�? *******************************************************************************
 */
#include <stdio.h>
#include <string.h>
#include "hc32_ll.h"
#include "boot_ota.h"
#include "boot_qspi.h"

/* 硬件自检开关（验证完可关） */
#ifndef BOOT_QSPI_DIAG
#define BOOT_QSPI_DIAG  1
#endif
/* 自检用最�?4KB 扇区�?x7FF000，避开暂存区与未来分区�?*/
#define BOOT_QSPI_TEST_SECTOR   0x007FF000UL
#define BOOT_QSPI_TEST_DATA     16UL

/* W25Q 指令补充 */
#define W25Q_READ_JEDEC_ID      0x9FU
#define W25Q_READ_STATUS2       0x35U
#define W25Q_SECTOR_ERASE       0x20U
#define W25Q_READ_DATA          0x03U
#define BOOT_QSPI_CHIP_SIZE     0x00800000UL   /* W25Q64 8MB */

/* ---- F460 JEUA QFN48 QSPI 引脚 ---- */
#define BOOT_QSPI_SCK_PORT   (GPIO_PORT_B)
#define BOOT_QSPI_SCK_PIN    (GPIO_PIN_14)
#define BOOT_QSPI_CS_PORT    (GPIO_PORT_B)
#define BOOT_QSPI_CS_PIN     (GPIO_PIN_01)
#define BOOT_QSPI_IO0_PORT   (GPIO_PORT_B)
#define BOOT_QSPI_IO0_PIN    (GPIO_PIN_13)
#define BOOT_QSPI_IO1_PORT   (GPIO_PORT_B)
#define BOOT_QSPI_IO1_PIN    (GPIO_PIN_12)
#define BOOT_QSPI_IO2_PORT   (GPIO_PORT_B)
#define BOOT_QSPI_IO2_PIN    (GPIO_PIN_10)
#define BOOT_QSPI_IO3_PORT   (GPIO_PORT_B)
#define BOOT_QSPI_IO3_PIN    (GPIO_PIN_02)
#define BOOT_QSPI_FUNC       (GPIO_FUNC_7)

/* Device ID�?x90 双字�?(MFR<<8)|DevID，与 App w25qxx.h 表一致） */
#define W25Q64_DEV_ID           0xEF16u   /* W25Q64  8MB (量产�? */
#define W25Q128_DEV_ID          0xEF17u   /* W25Q128 16MB (临时) */

/* W25Q 指令 */
#define W25Q_WRITE_ENABLE       0x06U
#define W25Q_WRITE_STATUS2      0x31U
#define W25Q_READ_STATUS1       0x05U
#define W25Q_PAGE_PROGRAM       0x02U

/**
 * @brief 直接指令模式发一串字�? */
static void qspi_cmd(const uint8_t *pu8Tx, uint32_t u32Len)
{
    uint32_t i;

    QSPI_EnterDirectCommMode();
    for (i = 0UL; i < u32Len; i++) {
        QSPI_WriteDirectCommValue(pu8Tx[i]);
    }
    QSPI_ExitDirectCommMode();
}

/**
 * @brief 轮询 SR1.BUSY 直到空闲
 */
static void qspi_wait_busy(void)
{
    uint32_t u32Cnt = 2000000UL;

    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(W25Q_READ_STATUS1);
    while (u32Cnt-- != 0UL) {
        if (0U == (QSPI_ReadDirectCommValue() & 0x01U)) {
            break;
        }
    }
    QSPI_ExitDirectCommMode();
}

/**
 * @brief 发送指�?可选数据，随后�?rxLen 字节（读 DCOM 自带时钟，逐字节收�? */
#if (BOOT_QSPI_DIAG == 1)   /* qspi_cmd_then_read 仅自检�?*/
static void qspi_cmd_then_read(const uint8_t *pu8Tx, uint32_t u32TxLen,
                               uint8_t *pu8Rx, uint32_t u32RxLen)
{
    uint32_t i;

    QSPI_EnterDirectCommMode();
    for (i = 0UL; i < u32TxLen; i++) {
        QSPI_WriteDirectCommValue(pu8Tx[i]);
    }
    for (i = 0UL; i < u32RxLen; i++) {
        pu8Rx[i] = QSPI_ReadDirectCommValue();
    }
    QSPI_ExitDirectCommMode();
}
#endif /* BOOT_QSPI_DIAG (qspi_cmd_then_read) */

/* �?SR1（验�?WEL/busy 用） */
#if (BOOT_QSPI_DIAG == 1)   /* qspi_read_sr1 仅自检�?*/
static uint8_t qspi_read_sr1(void)
{
    uint8_t u8Sr;

    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(W25Q_READ_STATUS1);
    u8Sr = QSPI_ReadDirectCommValue();
    QSPI_ExitDirectCommMode();
    return u8Sr;
}
#endif /* BOOT_QSPI_DIAG (qspi_read_sr1) */

/* 4KB 扇区擦除（需�?WREN�?*/
static void qspi_erase_4k(uint32_t u32Addr)
{
    uint8_t au8Cmd[4];
    uint8_t u8Wren = W25Q_WRITE_ENABLE;

    qspi_cmd(&u8Wren, 1U);
    au8Cmd[0] = W25Q_SECTOR_ERASE;
    au8Cmd[1] = (uint8_t)(u32Addr >> 16U);
    au8Cmd[2] = (uint8_t)(u32Addr >> 8U);
    au8Cmd[3] = (uint8_t)(u32Addr);
    qspi_cmd(au8Cmd, 4U);
    qspi_wait_busy();
}

/* 页编程单页（�?56B 且不�?256B 页边界；需�?WREN�?*/
static void qspi_program_page(uint32_t u32Addr, const uint8_t *pu8Data, uint32_t u32Len)
{
    uint8_t u8Wren = W25Q_WRITE_ENABLE;
    uint8_t au8Buf[260];
    uint32_t i;

    if ((u32Len == 0UL) || (u32Len > 256UL)) {
        return;
    }
    qspi_cmd(&u8Wren, 1U);
    au8Buf[0] = W25Q_PAGE_PROGRAM;
    au8Buf[1] = (uint8_t)(u32Addr >> 16U);
    au8Buf[2] = (uint8_t)(u32Addr >> 8U);
    au8Buf[3] = (uint8_t)(u32Addr);
    for (i = 0UL; i < u32Len; i++) {
        au8Buf[4UL + i] = pu8Data[i];
    }
    qspi_cmd(au8Buf, 4UL + u32Len);
    qspi_wait_busy();
}

/* 页编程（自动�?256B 页边界分页） */
static void qspi_program(uint32_t u32Addr, const uint8_t *pu8Data, uint32_t u32Len)
{
    while (u32Len > 0UL) {
        uint32_t u32Room = 256UL - (u32Addr & 0xFFUL);
        uint32_t u32N = (u32Len > u32Room) ? u32Room : u32Len;

        qspi_program_page(u32Addr, pu8Data, u32N);
        u32Addr += u32N;
        pu8Data += u32N;
        u32Len  -= u32N;
    }
}

/**
 * @brief 擦除 QSPI 区域�?KB 扇区粒度�? */
int boot_qspi_erase(uint32_t u32Addr, uint32_t u32Size)
{
    uint32_t u32End;

    if (u32Size == 0UL) {
        return -1;
    }
    u32End = u32Addr + u32Size;
    if (u32End > BOOT_QSPI_CHIP_SIZE) {
        return -1;
    }
    for (u32Addr &= ~0x0FFFU; u32Addr < u32End; u32Addr += 4096UL) {
        qspi_erase_4k(u32Addr);
    }
    return 0;
}

/**
 * @brief �?QSPI 区域（�?4KB 扇区：擦�?+ 256B 页编程）
 * @note  调用方保�?u32Addr 4KB 对齐（状态区/备份区均对齐�? */
int boot_qspi_write_region(uint32_t u32Addr, const uint8_t *pu8Src, uint32_t u32Size)
{
    uint32_t u32Off = 0UL;

    if ((pu8Src == NULL) || (u32Size == 0UL)) {
        return -1;
    }
    if ((u32Addr + u32Size) > BOOT_QSPI_CHIP_SIZE) {
        return -1;
    }
    while (u32Off < u32Size) {
        uint32_t u32SectAddr = u32Addr + u32Off;
        uint32_t u32N = u32Size - u32Off;
        uint32_t u32P = 0UL;

        if (u32N > (4096UL - (u32SectAddr & 0xFFFUL))) {
            u32N = 4096UL - (u32SectAddr & 0xFFFUL);
        }
        qspi_erase_4k(u32SectAddr & ~0xFFFUL);
        while (u32P < u32N) {
            uint32_t u32Room = 256UL - ((u32SectAddr + u32P) & 0xFFUL);
            uint32_t u32Np = u32N - u32P;

            if (u32Np > u32Room) {
                u32Np = u32Room;
            }
            qspi_program_page(u32SectAddr + u32P, pu8Src + u32Off + u32P, u32Np);
            u32P += u32Np;
        }
        u32Off += u32N;
    }
    return 0;
}

/**
 * @brief 初始�?QSPI 外设 + W25Q64 �?QE（易失，每次上电需重设�? */
int boot_qspi_init(void)
{
    stc_gpio_init_t stcGpioInit;
    stc_qspi_init_t stcQspiInit;
    uint8_t au8Cmd[2];

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDrv = PIN_HIGH_DRV;
    (void)GPIO_Init(BOOT_QSPI_SCK_PORT, BOOT_QSPI_SCK_PIN, &stcGpioInit);
    (void)GPIO_Init(BOOT_QSPI_CS_PORT,  BOOT_QSPI_CS_PIN,  &stcGpioInit);
    (void)GPIO_Init(BOOT_QSPI_IO0_PORT, BOOT_QSPI_IO0_PIN, &stcGpioInit);
    (void)GPIO_Init(BOOT_QSPI_IO1_PORT, BOOT_QSPI_IO1_PIN, &stcGpioInit);
    (void)GPIO_Init(BOOT_QSPI_IO2_PORT, BOOT_QSPI_IO2_PIN, &stcGpioInit);
    (void)GPIO_Init(BOOT_QSPI_IO3_PORT, BOOT_QSPI_IO3_PIN, &stcGpioInit);
    GPIO_SetFunc(BOOT_QSPI_SCK_PORT, BOOT_QSPI_SCK_PIN, BOOT_QSPI_FUNC);
    GPIO_SetFunc(BOOT_QSPI_CS_PORT,  BOOT_QSPI_CS_PIN,  BOOT_QSPI_FUNC);
    GPIO_SetFunc(BOOT_QSPI_IO0_PORT, BOOT_QSPI_IO0_PIN, BOOT_QSPI_FUNC);
    GPIO_SetFunc(BOOT_QSPI_IO1_PORT, BOOT_QSPI_IO1_PIN, BOOT_QSPI_FUNC);
    GPIO_SetFunc(BOOT_QSPI_IO2_PORT, BOOT_QSPI_IO2_PIN, BOOT_QSPI_FUNC);
    GPIO_SetFunc(BOOT_QSPI_IO3_PORT, BOOT_QSPI_IO3_PIN, BOOT_QSPI_FUNC);

    FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_QSPI, ENABLE);

    (void)QSPI_StructInit(&stcQspiInit);
    stcQspiInit.u32ClockDiv     = QSPI_CLK_DIV2;   /* �� app ͳһ��HCLK 200M/2 = 100MHz��W25Q64 �����ڣ� */
    stcQspiInit.u32ReadMode     = QSPI_RD_MD_QUAD_IO_FAST_RD;
    stcQspiInit.u32PrefetchMode = QSPI_PREFETCH_MD_EDGE_STOP;
    stcQspiInit.u32DummyCycle   = QSPI_DUMMY_CYCLE6;
    stcQspiInit.u32AddrWidth    = QSPI_ADDR_WIDTH_24BIT;
    stcQspiInit.u32SetupTime    = QSPI_QSSN_SETUP_ADVANCE_QSCK1P5;
    stcQspiInit.u32ReleaseTime  = QSPI_QSSN_RELEASE_DELAY_QSCK1P5;
    stcQspiInit.u32IntervalTime = QSPI_QSSN_INTERVAL_QSCK2;
    (void)QSPI_Init(&stcQspiInit);

    /* W25Q64JV: WREN + �?SR2(QE bit1) —�?每次上电需重设（状态寄存器易失�?*/
    au8Cmd[0] = W25Q_WRITE_ENABLE;
    qspi_cmd(au8Cmd, 1U);
    au8Cmd[0] = W25Q_WRITE_STATUS2;
    au8Cmd[1] = 0x02U;
    qspi_cmd(au8Cmd, 2U);
    qspi_wait_busy();
    return 0;
}

/**
 * @brief 纯编程（不擦除；调用方先 boot_qspi_erase）。自动按 256B 页边界分�? */
int boot_qspi_write(uint32_t u32Addr, const uint8_t *pu8Src, uint32_t u32Size)
{
    if ((pu8Src == NULL) || (u32Size == 0UL)) {
        return -1;
    }
    if ((u32Addr + u32Size) > BOOT_QSPI_CHIP_SIZE) {
        return -1;
    }
    qspi_program(u32Addr, pu8Src, u32Size);
    return 0;
}

/**
 * @brief 直接指令标准�?0x03)任意 QSPI 地址（已验证路径，不依赖 XIP�? * @note  0x03: instr + 3B 地址 + 连续读；�?DCOM 自带时钟
 */
int boot_qspi_read(uint32_t u32Addr, uint8_t *pu8Buf, uint32_t u32Size)
{
    uint32_t i;
    uint8_t u8Addr[3];

    if ((pu8Buf == NULL) || (u32Size == 0UL)) {
        return -1;
    }
    if ((u32Addr + u32Size) > BOOT_QSPI_CHIP_SIZE) {
        return -1;
    }
    u8Addr[0] = (uint8_t)(u32Addr >> 16U);
    u8Addr[1] = (uint8_t)(u32Addr >> 8U);
    u8Addr[2] = (uint8_t)(u32Addr);

    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(W25Q_READ_DATA);
    for (i = 0UL; i < 3UL; i++) {
        QSPI_WriteDirectCommValue(u8Addr[i]);
    }
    for (i = 0UL; i < u32Size; i++) {
        pu8Buf[i] = QSPI_ReadDirectCommValue();
    }
    QSPI_ExitDirectCommMode();
    return 0;
}


#if (BOOT_QSPI_DIAG == 1)
/**
 * @brief QSPI 硬件自检（printf）：JEDEC ID / QE / XIP �?/ 最后扇区擦写回�? * @note  执行一次擦写测试（写完回读比对后擦除还原），结果全打印
 */
void boot_qspi_diag(void)
{
    uint8_t au8Tx[1];
    uint8_t au8Jid[3];
    uint8_t u8Sr2;
    uint8_t au8Rd[16];
    uint8_t au8Pat[BOOT_QSPI_TEST_DATA];
    uint32_t i;
    int32_t i32Fail = 0;

    /* 1) JEDEC ID (0x9F)：EF 40 17=W25Q64 / EF 40 18=W25Q128（JEDEC 编码�?*/
    au8Tx[0] = W25Q_READ_JEDEC_ID;
    qspi_cmd_then_read(au8Tx, 1U, au8Jid, 3U);
    printf("QSPI JEDEC(9F): %02X %02X %02X\r\n",
           (unsigned)au8Jid[0], (unsigned)au8Jid[1], (unsigned)au8Jid[2]);

    /* 1b) Device ID (0x90，双字节) —�?�?App w25qxx.h 同款宏判�?*/
    {
        uint16_t u16DevId;

        QSPI_EnterDirectCommMode();
        QSPI_WriteDirectCommValue(0x90U);
        QSPI_WriteDirectCommValue(0x00U);   /* 24bit 地址 = 0 */
        QSPI_WriteDirectCommValue(0x00U);
        QSPI_WriteDirectCommValue(0x00U);
        au8Jid[0] = QSPI_ReadDirectCommValue();   /* MFR */
        au8Jid[1] = QSPI_ReadDirectCommValue();   /* DevID */
        QSPI_ExitDirectCommMode();
        u16DevId = (uint16_t)(((uint16_t)au8Jid[0] << 8U) | (uint16_t)au8Jid[1]);
        printf("QSPI DevID(90): %04X -> ", (unsigned)u16DevId);
        if (u16DevId == W25Q64_DEV_ID) {
            printf("W25Q64 (8MB, PROD)\r\n");
        } else if (u16DevId == W25Q128_DEV_ID) {
            printf("W25Q128 (16MB, TEMP)\r\n");
        } else {
            printf("unknown\r\n");
        }
    }

    /* 2) SR2 QE 位（bit1，期�?1——init 已置位） */
    au8Tx[0] = W25Q_READ_STATUS2;
    qspi_cmd_then_read(au8Tx, 1U, &u8Sr2, 1U);
    printf("QSPI SR2:    %02X (QE=%u)\r\n", (unsigned)u8Sr2,
           (unsigned)((u8Sr2 >> 1U) & 0x01U));

    /* 3) XIP 读暂存区�?16B（新片应�?FF FF ...�?*/
    (void)boot_qspi_read(BOOT_QSPI_STAGE_BASE, au8Rd, sizeof(au8Rd));
    printf("QSPI XIP @%06X: ", (unsigned)BOOT_QSPI_STAGE_BASE);
    for (i = 0UL; i < sizeof(au8Rd); i++) {
        printf("%02X ", (unsigned)au8Rd[i]);
    }
    printf("\r\n");

    /* 4) 分步擦写测试 @0x7FF000（fresh �?FF，可独立验证 编程/擦除�?*/
    for (i = 0UL; i < sizeof(au8Pat); i++) {
        au8Pat[i] = (uint8_t)(0xA0U + i);
    }

    /* A) WREN 锁存检查：0x06 �?SR1.WEL(bit1) 应为 1 */
    {
        uint8_t u8Wren = W25Q_WRITE_ENABLE;
        qspi_cmd(&u8Wren, 1U);
        printf("QSPI SR1 after WREN: %02X (WEL=%u, expect 1)\r\n",
               (unsigned)qspi_read_sr1(), (unsigned)((qspi_read_sr1() >> 1U) & 0x01U));
    }

    /* B) 编程 0x00（fresh FF 上仅清位，无需擦除�?> 直读应全 00 */
    for (i = 0UL; i < sizeof(au8Rd); i++) {
        au8Rd[i] = 0x00U;
    }
    qspi_program(BOOT_QSPI_TEST_SECTOR, au8Rd, sizeof(au8Rd));
    (void)boot_qspi_read(BOOT_QSPI_TEST_SECTOR, au8Pat, sizeof(au8Pat));
    i32Fail = 0;
    for (i = 0UL; i < sizeof(au8Pat); i++) {
        if (au8Pat[i] != 0x00U) {
            i32Fail = 1;
            break;
        }
    }
    printf("QSPI program-00 test: %s", (i32Fail == 0) ? "PASS" : "FAIL");
    if (i32Fail != 0) {
        printf("  got: ");
        for (i = 0UL; i < sizeof(au8Pat); i++) {
            printf("%02X ", (unsigned)au8Pat[i]);
        }
    }
    printf("\r\n");

    /* C) 擦除 -> 直读应回 FF（证明擦除生效） */
    qspi_erase_4k(BOOT_QSPI_TEST_SECTOR);
    (void)boot_qspi_read(BOOT_QSPI_TEST_SECTOR, au8Pat, sizeof(au8Pat));
    i32Fail = 0;
    for (i = 0UL; i < sizeof(au8Pat); i++) {
        if (au8Pat[i] != 0xFFU) {
            i32Fail = 1;
            break;
        }
    }
    printf("QSPI erase test:       %s", (i32Fail == 0) ? "PASS" : "FAIL");
    if (i32Fail != 0) {
        printf("  got: ");
        for (i = 0UL; i < sizeof(au8Pat); i++) {
            printf("%02X ", (unsigned)au8Pat[i]);
        }
    }
    printf("\r\n");

    /* D) 擦除后重新编程图案并回读比对（完�?RW�?*/
    for (i = 0UL; i < sizeof(au8Pat); i++) {
        au8Pat[i] = (uint8_t)(0xA0U + i);
    }
    qspi_program(BOOT_QSPI_TEST_SECTOR, au8Pat, sizeof(au8Pat));
    (void)boot_qspi_read(BOOT_QSPI_TEST_SECTOR, au8Rd, sizeof(au8Rd));
    i32Fail = 0;
    for (i = 0UL; i < sizeof(au8Pat); i++) {
        if (au8Rd[i] != au8Pat[i]) {
            i32Fail = 1;
            break;
        }
    }
    printf("QSPI RW test @%06X:   %s\r\n", (unsigned)BOOT_QSPI_TEST_SECTOR,
           (i32Fail == 0) ? "PASS" : "FAIL");
    if (i32Fail != 0) {
        printf("  exp: ");
        for (i = 0UL; i < sizeof(au8Pat); i++) {
            printf("%02X ", (unsigned)au8Pat[i]);
        }
        printf("\r\n  got: ");
        for (i = 0UL; i < sizeof(au8Rd); i++) {
            printf("%02X ", (unsigned)au8Rd[i]);
        }
        printf("\r\n");
    }
    qspi_erase_4k(BOOT_QSPI_TEST_SECTOR);   /* 还原�?FF */
    printf("QSPI diag done\r\n");
}

#endif /* BOOT_QSPI_DIAG */
/******************************************************************************
 * EOF
 *****************************************************************************/
