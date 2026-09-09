// stm32f1_uart.c —— USART1 PA9/PA10 115200 8N1 轮询发送
#include "stm32f1_uart.h"
#include "stm32f1_gpio.h"

void uart1_init(void) {
    // 开 GPIOA 与 USART1 时钟
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN | RCC_APB2ENR_AFIOEN;

    // PA9 = 复用推挽 50MHz, PA10 = 浮空输入
    gpio_set_mode(GPIOA, 9, GPIO_MODE_AF_PP_50);
    gpio_set_mode(GPIOA, 10, GPIO_MODE_INPUT_FLOATING);

    // USART 配置: 115200 8N1, TE | RE | UE
    // BRR = 72MHz / 115200 = 625 = 0x271 (整数部分 625, 小数 0)
    USART1->BRR = 625;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

void uart1_putc(char c) {
    // 等待 TXE
    while (!(USART1->SR & USART_SR_TXE)) { }
    USART1->DR = (uint8_t)c;
}

void uart1_puts(const char *s) {
    while (*s) {
        uart1_putc(*s++);
    }
}

void uart1_write(const char *buf, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uart1_putc(buf[i]);
    }
}

// === P7: UART 接收函数 ===

// 非阻塞读取:检查 RXNE 标志,有数据返回字节,无数据返回 -1
int uart1_getc_nonblock(void) {
    if (USART1->SR & USART_SR_RXNE) {
        return (int)(USART1->DR & 0xFF);
    }
    return -1;
}

// 带超时的阻塞读取:轮询 RXNE,timeout_loop 为循环计数上限
// 注:本函数为忙等待,适用于固件下载等需要快速逐字节接收的场景
int uart1_getc_timeout(uint32_t timeout_loop) {
    while (timeout_loop--) {
        if (USART1->SR & USART_SR_RXNE) {
            return (int)(USART1->DR & 0xFF);
        }
    }
    return -1;
}
