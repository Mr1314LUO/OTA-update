// stm32f1_spi.c —— SPI1 轮询驱动
// PA5=SCK, PA6=MISO, PA7=MOSI, PA4=CS(GPIO 软件控制)
#include "stm32f1_spi.h"
#include "stm32f1_gpio.h"

void spi1_init(void) {
    // 开 GPIOA + SPI1 时钟
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_SPI1EN | RCC_APB2ENR_AFIOEN;

    // PA5=SCK 复用推挽 50MHz, PA7=MOSI 复用推挽 50MHz
    gpio_set_mode(GPIOA, 5, GPIO_MODE_AF_PP_50);
    gpio_set_mode(GPIOA, 7, GPIO_MODE_AF_PP_50);
    // PA6=MISO 浮空输入
    gpio_set_mode(GPIOA, 6, GPIO_MODE_INPUT_FLOATING);
    // PA4=CS 推挽输出 50MHz,默认高(未选中)
    gpio_set_mode(GPIOA, 4, GPIO_MODE_OUTPUT_PP_50);
    gpio_set(GPIOA, 4);  // CS=high

    // 配置 SPI1 CR1: 主机, mode 0, MSB first, /4 分频, SSM+SSI
    // BR=001(/4), MSTR=1, CPOL=0, CPHA=0, SSM=1, SSI=1
    SPI1->CR1 = 0;
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI
              | (1UL << 3);  // BR=001 → /4 = 18MHz
    SPI1->CR1 |= SPI_CR1_SPE;  // 使能 SPI
}

void spi1_cs_low(void)  { gpio_reset(GPIOA, 4); }
void spi1_cs_high(void) { gpio_set(GPIOA, 4); }

uint8_t spi1_transfer(uint8_t tx) {
    // 等待 TXE(发送缓冲区空)
    while (!(SPI1->SR & SPI_SR_TXE)) { }
    SPI1->DR = tx;
    // 等待 RXNE(接收缓冲区非空)
    while (!(SPI1->SR & SPI_SR_RXNE)) { }
    return (uint8_t)SPI1->DR;
}
