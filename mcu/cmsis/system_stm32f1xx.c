// system_stm32f1xx.c —— SystemInit: HSE 8MHz × PLL9 = 72MHz, Flash latency 2 WS
// 不依赖完整 CMSIS,只操作项目所用寄存器
#include "stm32f103xb.h"

uint32_t SystemCoreClock = 72000000UL;

// 由 Reset_Handler 调用,早于 main,不等 systick
void SystemInit(void) {
    // 关 PLL,切回 HSI(避免配置时干扰)
    RCC->CFGR &= ~(RCC_CFGR_SW_Msk);
    while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != 0) { }  // 等 HSI 作为 SYSCLK

    // 关 PLL 再配置
    RCC->CR &= ~RCC_CR_PLLON;

    // 开 HSE, 等就绪
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY)) { }

    // Flash prefetch + 2 wait states(72MHz 必须 2 WS)
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;

    // 配置总线分频:HCLK=72MHz, PCLK1=36MHz(/2), PCLK2=72MHz(/1), ADC=12MHz(/6)
    RCC->CFGR = 0;
    RCC->CFGR |= (0x4UL << 8)    // PPRE1 = /2  (100)
              |  (0x0UL << 11)   // PPRE2 = /1  (0xx)
              |  RCC_CFGR_PLLSRC  // PLL 源 = HSE
              |  RCC_CFGR_PLLMULL9; // PLL ×9 = 72MHz
    // ADC 预分频保持默认(由 PCLK2/6 决定, 不动 ADCPRE)

    // 开 PLL, 等就绪
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) { }

    // 切 SYSCLK 到 PLL
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_PLL) { }

    SystemCoreClock = 72000000UL;
}

// SystemCoreClockUpdate(可选,项目里不用)
void SystemCoreClockUpdate(void) {
    SystemCoreClock = 72000000UL;
}
