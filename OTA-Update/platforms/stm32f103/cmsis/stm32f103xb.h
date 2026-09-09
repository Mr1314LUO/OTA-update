// stm32f103xb.h —— STM32F103C8T6 最小化设备头(自包含,不依赖完整 CMSIS)
// 仅定义本项目用到的外设与寄存器位,适合 16KB Bootloader / 48KB App
#ifndef STM32F103XB_H
#define STM32F103XB_H

#include <stdint.h>

// ---------- 内核寄存器(Cortex-M3) ----------
#define SCB_BASE              (0xE000ED00UL)
#define SCB                   ((SCB_Type *)SCB_BASE)
#define SysTick_BASE          (0xE000E010UL)
#define SysTick               ((SysTick_Type *)SysTick_BASE)
#define NVIC_BASE             (0xE000E100UL)
#define NVIC                   ((NVIC_Type *)NVIC_BASE)

typedef struct {
    volatile uint32_t CPUID, ICSR, VTOR, AIRCR, SCR, CCR, SHPR[12], SHCSR, CFSR, HFSR, DFSR, MMFAR, BFAR, AFSR, PFR[2], DFR, ADR, MMFR[4], ISAR[5];
    uint32_t RESERVED0[5];
    volatile uint32_t CPACR;
} SCB_Type;

typedef struct {
    volatile uint32_t CTRL, LOAD, VAL, CALIB;
} SysTick_Type;

typedef struct {
    volatile uint32_t ISER[8];
    uint32_t RESERVED0[24];
    volatile uint32_t ICER[8];
    uint32_t RESERVED1[24];
    volatile uint32_t ISPR[8];
    uint32_t RESERVED2[24];
    volatile uint32_t ICPR[8];
    uint32_t RESERVED3[24];
    volatile uint32_t IABR[8];
    uint32_t RESERVED4[56];
    volatile uint8_t IP[240];
    uint32_t RESERVED5[2];
    volatile uint32_t STIR;
} NVIC_Type;

// SCB_AIRCR 复位键
#define SCB_AIRCR_VECTKEY_Pos         16
#define SCB_AIRCR_VECTKEY_MASK        (0xFFFFUL << SCB_AIRCR_VECTKEY_Pos)
#define SCB_AIRCR_PRIGROUP_Pos        8
#define SCB_AIRCR_SYSRESETREQ_Pos     2
#define SCB_AIRCR_SYSRESETREQ_Msk     (1UL << SCB_AIRCR_SYSRESETREQ_Pos)

// SysTick
#define SysTick_CTRL_ENABLE_Msk       (1UL << 0)
#define SysTick_CTRL_TICKINT_Msk      (1UL << 1)
#define SysTick_CTRL_CLKSOURCE_Msk    (1UL << 2)

// ---------- 外设寄存器基址 ----------
#define PERIPH_BASE           (0x40000000UL)
#define APB1PERIPH_BASE       (PERIPH_BASE)
#define APB2PERIPH_BASE       (PERIPH_BASE + 0x10000UL)
#define AHBPERIPH_BASE        (PERIPH_BASE + 0x20000UL)

#define GPIOA_BASE            (APB2PERIPH_BASE + 0x0800UL)
#define GPIOB_BASE            (APB2PERIPH_BASE + 0x0C00UL)
#define GPIOC_BASE            (APB2PERIPH_BASE + 0x1000UL)
#define GPIOD_BASE            (APB2PERIPH_BASE + 0x1400UL)

#define RCC_BASE              (AHBPERIPH_BASE + 0x1000UL)
#define FLASH_R_BASE          (AHBPERIPH_BASE + 0x2000UL)
#define USART1_BASE           (APB2PERIPH_BASE + 0x3800UL)
#define SPI1_BASE             (APB2PERIPH_BASE + 0x3000UL)

// ---------- GPIO ----------
typedef struct {
    volatile uint32_t CRL, CRH, IDR, ODR, BSRR, BRR, LCKR;
} GPIO_Type;
#define GPIOA                 ((GPIO_Type *)GPIOA_BASE)
#define GPIOB                 ((GPIO_Type *)GPIOB_BASE)
#define GPIOC                 ((GPIO_Type *)GPIOC_BASE)
#define GPIOD                 ((GPIO_Type *)GPIOD_BASE)

// ---------- RCC ----------
typedef struct {
    volatile uint32_t CR, CFGR, CIR, APB2RSTR, APB1RSTR, AHBENR, APB2ENR, APB1ENR, BDCR, CSR;
} RCC_Type;
#define RCC                   ((RCC_Type *)RCC_BASE)

// RCC_CR 位
#define RCC_CR_HSION                  (1UL << 0)
#define RCC_CR_HSIRDY                 (1UL << 1)
#define RCC_CR_HSEON                  (1UL << 16)
#define RCC_CR_HSERDY                 (1UL << 17)
#define RCC_CR_HSEBYP                 (1UL << 18)
#define RCC_CR_PLLON                  (1UL << 24)
#define RCC_CR_PLLRDY                 (1UL << 25)

// RCC_CFGR 位
#define RCC_CFGR_SW_HSI               (0x0UL)
#define RCC_CFGR_SW_HSE               (0x1UL)
#define RCC_CFGR_SW_PLL               (0x2UL)
#define RCC_CFGR_SW_Msk               (0x3UL)
#define RCC_CFGR_SWS_Msk              (0x3UL << 2)
#define RCC_CFGR_SWS_PLL              (0x2UL << 2)
#define RCC_CFGR_HPRE_Msk             (0xFUL << 4)
#define RCC_CFGR_PPRE1_Msk            (0x7UL << 8)
#define RCC_CFGR_PPRE2_Msk            (0x7UL << 11)
#define RCC_CFGR_PLLMULL_Msk          (0xFUL << 18)
#define RCC_CFGR_PLLMULL9             (0x7UL << 18)
#define RCC_CFGR_PLLSRC               (1UL << 16)
#define RCC_CFGR_ADCPRE_Msk           (0xFUL << 14)
#define RCC_CFGR_USBPRE                (1UL << 22)

// RCC AHBENR
#define RCC_AHBENR_DMA1EN             (1UL << 0)
#define RCC_AHBENR_CRCEN              (1UL << 6)
#define RCC_AHBENR_FLITFEN            (1UL << 4)

// RCC APB1ENR
#define RCC_APB1ENR_USART2EN          (1UL << 17)
#define RCC_APB1ENR_USART3EN          (1UL << 18)

// RCC APB2ENR
#define RCC_APB2ENR_AFIOEN            (1UL << 0)
#define RCC_APB2ENR_IOPAEN            (1UL << 2)
#define RCC_APB2ENR_IOPBEN            (1UL << 3)
#define RCC_APB2ENR_IOPCEN            (1UL << 4)
#define RCC_APB2ENR_IOPDEN            (1UL << 5)
#define RCC_APB2ENR_USART1EN          (1UL << 14)
#define RCC_APB2ENR_SPI1EN            (1UL << 12)

// ---------- Flash 控制寄存器 ----------
typedef struct {
    volatile uint32_t ACR, KEYR, OPTKEYR, SR, CR, AR, RESERVED, OBR, WRPR;
} FLASH_Type;
#define FLASH                ((FLASH_Type *)FLASH_R_BASE)

// FLASH_ACR
#define FLASH_ACR_PRFTBE              (1UL << 4)
#define FLASH_ACR_LATENCY_Msk         (0x7UL << 0)
#define FLASH_ACR_LATENCY_2           (2UL << 0)

// FLASH_SR
#define FLASH_SR_BSY                  (1UL << 0)
#define FLASH_SR_PGERR                (1UL << 2)
#define FLASH_SR_WRPRTERR             (1UL << 4)
#define FLASH_SR_EOP                  (1UL << 5)

// FLASH_CR
#define FLASH_CR_PG                   (1UL << 0)
#define FLASH_CR_PER                  (1UL << 1)
#define FLASH_CR_MER                  (1UL << 2)
#define FLASH_CR_STRT                 (1UL << 6)
#define FLASH_CR_LOCK                 (1UL << 7)

// Flash 解锁键序列
#define FLASH_KEY1                    (0x45670123UL)
#define FLASH_KEY2                    (0xCDEF89ABUL)

// ---------- USART ----------
typedef struct {
    volatile uint32_t SR, DR, BRR, CR1, CR2, CR3, GTPR;
} USART_Type;
#define USART1                ((USART_Type *)USART1_BASE)

// USART_SR
#define USART_SR_PE                    (1UL << 0)
#define USART_SR_TXE                   (1UL << 7)
#define USART_SR_TC                    (1UL << 6)
#define USART_SR_RXNE                  (1UL << 5)

// USART_CR1
#define USART_CR1_UE                   (1UL << 13)
#define USART_CR1_TE                   (1UL << 3)
#define USART_CR1_RE                   (1UL << 2)

// ---------- SPI ----------
typedef struct {
    volatile uint32_t CR1, CR2, SR, DR, CRCPR, RXCRCR, TXCRCR, I2SCFGR, I2SPR;
} SPI_Type;
#define SPI1                  ((SPI_Type *)SPI1_BASE)

// SPI_CR1
#define SPI_CR1_CPHA                   (1UL << 0)
#define SPI_CR1_CPOL                   (1UL << 1)
#define SPI_CR1_MSTR                   (1UL << 2)
#define SPI_CR1_BR_Msk                 (0x7UL << 3)
#define SPI_CR1_SPE                    (1UL << 6)
#define SPI_CR1_LSBFIRST               (1UL << 7)
#define SPI_CR1_SSI                    (1UL << 8)
#define SPI_CR1_SSM                    (1UL << 9)
#define SPI_CR1_BIDIMODE               (1UL << 15)

// SPI_CR2
#define SPI_CR2_SSOE                   (1UL << 2)

// SPI_SR
#define SPI_SR_RXNE                    (1UL << 0)
#define SPI_SR_TXE                     (1UL << 1)
#define SPI_SR_BSY                     (1UL << 7)

// ---------- IRQn 枚举 ----------
typedef enum {
    // Cortex-M3 内核异常
    Reset_IRQn = -15, NMI_IRQn = -14, HardFault_IRQn = -13,
    MemoryManagement_IRQn = -12, BusFault_IRQn = -11, UsageFault_IRQn = -10,
    SVCall_IRQn = -5, DebugMonitor_IRQn = -4, PendSV_IRQn = -2, SysTick_IRQn = -1,
    // F103 外设中断
    WWDG_IRQn = 0, PVD_IRQn = 1, TAMPER_IRQn = 2, RTC_IRQn = 3, FLASH_IRQn = 4,
    RCC_IRQn = 5, EXTI0_IRQn = 6, EXTI1_IRQn = 7, EXTI2_IRQn = 8, EXTI3_IRQn = 9,
    EXTI4_IRQn = 10, DMA1_Channel1_IRQn = 11, DMA1_Channel2_IRQn = 12, DMA1_Channel3_IRQn = 13,
    DMA1_Channel4_IRQn = 14, DMA1_Channel5_IRQn = 15, DMA1_Channel6_IRQn = 16, DMA1_Channel7_IRQn = 17,
    ADC1_2_IRQn = 18, USB_HP_CAN_TX_IRQn = 19, USB_LP_CAN_RX0_IRQn = 20, CAN_RX1_IRQn = 21,
    CAN_SCE_IRQn = 22, EXTI9_5_IRQn = 23, TIM1_BRK_IRQn = 24, TIM1_UP_IRQn = 25,
    TIM1_TRG_COM_IRQn = 26, TIM1_CC_IRQn = 27, TIM2_IRQn = 28, TIM3_IRQn = 29,
    TIM4_IRQn = 30, I2C1_EV_IRQn = 31, I2C1_ER_IRQn = 32, I2C2_EV_IRQn = 33,
    I2C2_ER_IRQn = 34, SPI1_IRQn = 35, SPI2_IRQn = 36, USART1_IRQn = 37,
    USART2_IRQn = 38, USART3_IRQn = 39, EXTI15_10_IRQn = 40, RTCAlarm_IRQn = 41,
    USBWakeUp_IRQn = 42
} IRQn_Type;

// ---------- 工具函数 ----------
// 注意:arm-none-eabi-gcc 12.2.1 工具链自带 CMSIS-Core 内联函数 __WFI/__DSB/__enable_irq
// 等(通过 cmsis_gcc.h 隐式引入),这里不重复定义以避免冲突。
// 如果工具链不提供,请改用裸 __asm volatile("wfi") 等代替。

// NVIC_SystemReset 自定义(工具链通常不提供)
static inline void NVIC_SystemReset(void) {
    __asm volatile ("dsb 0xF" ::: "memory");
    SCB->AIRCR = ((0x5FAUL << 16) | SCB_AIRCR_SYSRESETREQ_Msk);
    __asm volatile ("dsb 0xF" ::: "memory");
    while (1) { }
}

#endif // STM32F103xB_H
