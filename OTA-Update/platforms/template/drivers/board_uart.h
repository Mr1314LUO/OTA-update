// board_uart.h —— 平台 UART 驱动契约（模板）
//
// 用途：
//   1. printf_lite.c 通过 board_uart_putc() 输出所有日志
//   2. app/main.c 的固件下载命令通过 board_uart_getc_*() 接收数据
// 你需要在 board_uart.c 中按芯片手册实现以下函数（配置串口、GPIO 复用、波特率）。
// 参考：platforms/stm32f103/drivers/stm32f1_uart.c
#ifndef BOARD_UART_H
#define BOARD_UART_H

#include <stdint.h>

// 初始化调试串口（建议 115200 8N1），同时配置 TX/RX 引脚复用
void board_uart_init(void);

// 阻塞发送一个字节（等 TX 寄存器空 → 写数据）
void board_uart_putc(char c);

// 非阻塞读取一个字节：有数据返回字节值(0-255)，无数据返回 -1
int board_uart_getc_nonblock(void);

// 带超时的阻塞读取：超时返回 -1，成功返回字节值(0-255)
// timeout_us 为微秒级超时（内部可用 SysTick/定时器或轮询计数实现）
int board_uart_getc_timeout(uint32_t timeout_us);

#endif // BOARD_UART_H
