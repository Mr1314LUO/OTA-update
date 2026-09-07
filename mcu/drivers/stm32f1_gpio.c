// stm32f1_gpio.c —— GPIO 模式配置
#include "stm32f1_gpio.h"

void gpio_set_mode(GPIO_Type *port, uint8_t pin, uint8_t mode) {
    if (pin < 8) {
        uint32_t shift = pin * 4;
        uint32_t mask = 0xFUL << shift;
        port->CRL = (port->CRL & ~mask) | ((uint32_t)mode << shift);
    } else {
        uint32_t shift = (pin - 8) * 4;
        uint32_t mask = 0xFUL << shift;
        port->CRH = (port->CRH & ~mask) | ((uint32_t)mode << shift);
    }
}
