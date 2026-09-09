// stm32f1_gpio.h —— 最小化 GPIO 工具(寄存器级,免 CMSIS HAL)
#ifndef STM32F1_GPIO_H
#define STM32F1_GPIO_H
#include "stm32f103xb.h"

// STM32F1 GPIO 模式(CRL/CRH 每位 4 bit: MODE[1:0] + CNF[1:0])
#define GPIO_MODE_INPUT          0x00  // CNF=00 + MODE=00
#define GPIO_MODE_OUTPUT_PP_10   0x01  // 推挽输出 10MHz
#define GPIO_MODE_OUTPUT_PP_50   0x03  // 推挽输出 50MHz
#define GPIO_MODE_AF_PP_50       0xB   // 复用推挽 50MHz
#define GPIO_MODE_INPUT_PUPD     0x08  // 上拉/下拉输入(CNF=10)
#define GPIO_MODE_INPUT_ANALOG   0x00  // CNF=00 + MODE=00 + 无上下拉
#define GPIO_MODE_INPUT_FLOATING 0x04  // CNF=01 + MODE=00

// 设置 GPIO 引脚模式(pin 范围 0-15)
void gpio_set_mode(GPIO_Type *port, uint8_t pin, uint8_t mode);

// 设置/清除引脚(BSRR)
static inline void gpio_set(GPIO_Type *port, uint8_t pin) {
    port->BSRR = (1UL << pin);
}
static inline void gpio_reset(GPIO_Type *port, uint8_t pin) {
    port->BSRR = (1UL << (pin + 16));
}
static inline void gpio_toggle(GPIO_Type *port, uint8_t pin) {
    port->ODR ^= (1UL << pin);
}

#endif
