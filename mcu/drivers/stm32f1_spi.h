// stm32f1_spi.h —— SPI1 轮询驱动(PA5 SCK / PA6 MISO / PA7 MOSI / PA4 CS)
#ifndef STM32F1_SPI_H
#define STM32F1_SPI_H
#include "stm32f103xb.h"
#include <stdint.h>

// 初始化 SPI1 主机模式 0,18MHz(72MHz/4),软件 CS
void spi1_init(void);

// CS 控制(PA4)
void spi1_cs_low(void);
void spi1_cs_high(void);

// 全双工传输一字节,返回接收字节
uint8_t spi1_transfer(uint8_t tx);

#endif
