// stm32f1_uart.h —— USART1 (PA9 TX / PA10 RX) 轮询发送
#ifndef STM32F1_UART_H
#define STM32F1_UART_H
#include "stm32f103xb.h"
#include <stdint.h>

// 初始化 USART1 115200 8N1(同时配置 PA9/PA10 复用)
void uart1_init(void);

// 阻塞发送一个字节
void uart1_putc(char c);

// 阻塞发送字符串
void uart1_puts(const char *s);

// 阻塞发送带长度
void uart1_write(const char *buf, uint32_t len);

// === P7: UART 接收函数 ===

// 非阻塞读取一个字节:有数据返回字节值(0-255),无数据返回 -1
int uart1_getc_nonblock(void);

// 带超时的阻塞读取:超时返回 -1,成功返回字节值(0-255)
// timeout_loop 为轮询循环计数(非毫秒),约 72MHz 下每循环 ~7ns
int uart1_getc_timeout(uint32_t timeout_loop);

#endif
