@ startup_stm32f103xb.s —— STM32F103C8T6 启动文件(Cortex-M3)
@ 中断向量表 + Reset_Handler 复制 .data/.bss + 调 SystemInit + main
@ SysTick/PendSV/SVC 用 .weak 让 stm32f1xx_it.c 覆盖(FreeRTOS port)
    .syntax unified
    .cpu cortex-m3
    .thumb

@ ==================== 栈顶符号(链接脚本提供) ====================
.extern _estack
.extern _sidata
.extern _sdata
.extern _edata
.extern _sbss
.extern _ebss

@ ==================== 弱符号默认处理 ====================
.weak NMI_Handler
.thumb_set NMI_Handler, Default_Handler
.weak HardFault_Handler
.thumb_set HardFault_Handler, Default_Handler
.weak MemManage_Handler
.thumb_set MemManage_Handler, Default_Handler
.weak BusFault_Handler
.thumb_set BusFault_Handler, Default_Handler
.weak UsageFault_Handler
.thumb_set UsageFault_Handler, Default_Handler
.weak SVC_Handler
.thumb_set SVC_Handler, Default_Handler
.weak DebugMon_Handler
.thumb_set DebugMon_Handler, Default_Handler
.weak PendSV_Handler
.thumb_set PendSV_Handler, Default_Handler
.weak SysTick_Handler
.thumb_set SysTick_Handler, Default_Handler

@ 其他外设中断默认弱符号(全指向 Default_Handler)
.macro DEFAULT_IRQ name
.weak \name
.thumb_set \name, Default_Handler
.endm
DEFAULT_IRQ WWDG_IRQHandler
DEFAULT_IRQ PVD_IRQHandler
DEFAULT_IRQ TAMPER_IRQHandler
DEFAULT_IRQ RTC_IRQHandler
DEFAULT_IRQ FLASH_IRQHandler
DEFAULT_IRQ RCC_IRQHandler
DEFAULT_IRQ EXTI0_IRQHandler
DEFAULT_IRQ EXTI1_IRQHandler
DEFAULT_IRQ EXTI2_IRQHandler
DEFAULT_IRQ EXTI3_IRQHandler
DEFAULT_IRQ EXTI4_IRQHandler
DEFAULT_IRQ DMA1_Channel1_IRQHandler
DEFAULT_IRQ DMA1_Channel2_IRQHandler
DEFAULT_IRQ DMA1_Channel3_IRQHandler
DEFAULT_IRQ DMA1_Channel4_IRQHandler
DEFAULT_IRQ DMA1_Channel5_IRQHandler
DEFAULT_IRQ DMA1_Channel6_IRQHandler
DEFAULT_IRQ DMA1_Channel7_IRQHandler
DEFAULT_IRQ ADC1_2_IRQHandler
DEFAULT_IRQ USB_HP_CAN_TX_IRQHandler
DEFAULT_IRQ USB_LP_CAN_RX0_IRQHandler
DEFAULT_IRQ CAN_RX1_IRQHandler
DEFAULT_IRQ CAN_SCE_IRQHandler
DEFAULT_IRQ EXTI9_5_IRQHandler
DEFAULT_IRQ TIM1_BRK_IRQHandler
DEFAULT_IRQ TIM1_UP_IRQHandler
DEFAULT_IRQ TIM1_TRG_COM_IRQHandler
DEFAULT_IRQ TIM1_CC_IRQHandler
DEFAULT_IRQ TIM2_IRQHandler
DEFAULT_IRQ TIM3_IRQHandler
DEFAULT_IRQ TIM4_IRQHandler
DEFAULT_IRQ I2C1_EV_IRQHandler
DEFAULT_IRQ I2C1_ER_IRQHandler
DEFAULT_IRQ I2C2_EV_IRQHandler
DEFAULT_IRQ I2C2_ER_IRQHandler
DEFAULT_IRQ SPI1_IRQHandler
DEFAULT_IRQ SPI2_IRQHandler
DEFAULT_IRQ USART1_IRQHandler
DEFAULT_IRQ USART2_IRQHandler
DEFAULT_IRQ USART3_IRQHandler
DEFAULT_IRQ EXTI15_10_IRQHandler
DEFAULT_IRQ RTCAlarm_IRQHandler
DEFAULT_IRQ USBWakeUp_IRQHandler

@ ==================== 向量表 ====================
.section .isr_vector,"a",%progbits
.global g_pfnVectors
.type g_pfnVectors, %object
g_pfnVectors:
    .word _estack                  @ 初始 SP
    .word Reset_Handler            @ 1: Reset
    .word NMI_Handler              @ 2: NMI
    .word HardFault_Handler        @ 3: HardFault
    .word MemManage_Handler        @ 4: MemManage
    .word BusFault_Handler         @ 5: BusFault
    .word UsageFault_Handler      @ 6: UsageFault
    .word 0                        @ 7: Reserved
    .word 0                        @ 8: Reserved
    .word 0                        @ 9: Reserved
    .word 0                        @ 10: Reserved
    .word vPortSVCHandler          @ 11: SVC (FreeRTOS port)
    .word DebugMon_Handler        @ 12: DebugMon
    .word 0                        @ 13: Reserved
    .word xPortPendSVHandler       @ 14: PendSV (FreeRTOS port)
    .word xPortSysTickHandler      @ 15: SysTick (FreeRTOS port)
    @ 外设中断
    .word WWDG_IRQHandler
    .word PVD_IRQHandler
    .word TAMPER_IRQHandler
    .word RTC_IRQHandler
    .word FLASH_IRQHandler
    .word RCC_IRQHandler
    .word EXTI0_IRQHandler
    .word EXTI1_IRQHandler
    .word EXTI2_IRQHandler
    .word EXTI3_IRQHandler
    .word EXTI4_IRQHandler
    .word DMA1_Channel1_IRQHandler
    .word DMA1_Channel2_IRQHandler
    .word DMA1_Channel3_IRQHandler
    .word DMA1_Channel4_IRQHandler
    .word DMA1_Channel5_IRQHandler
    .word DMA1_Channel6_IRQHandler
    .word DMA1_Channel7_IRQHandler
    .word ADC1_2_IRQHandler
    .word USB_HP_CAN_TX_IRQHandler
    .word USB_LP_CAN_RX0_IRQHandler
    .word CAN_RX1_IRQHandler
    .word CAN_SCE_IRQHandler
    .word EXTI9_5_IRQHandler
    .word TIM1_BRK_IRQHandler
    .word TIM1_UP_IRQHandler
    .word TIM1_TRG_COM_IRQHandler
    .word TIM1_CC_IRQHandler
    .word TIM2_IRQHandler
    .word TIM3_IRQHandler
    .word TIM4_IRQHandler
    .word I2C1_EV_IRQHandler
    .word I2C1_ER_IRQHandler
    .word I2C2_EV_IRQHandler
    .word I2C2_ER_IRQHandler
    .word SPI1_IRQHandler
    .word SPI2_IRQHandler
    .word USART1_IRQHandler
    .word USART2_IRQHandler
    .word USART3_IRQHandler
    .word EXTI15_10_IRQHandler
    .word RTCAlarm_IRQHandler
    .word USBWakeUp_IRQHandler
    .size g_pfnVectors, . - g_pfnVectors

@ ==================== Reset_Handler ====================
.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    ldr   r0, =_estack
    mov   sp, r0                  @ 设 MSP

    @ 复制 .data 从 FLASH 到 RAM
    ldr   r0, =_sidata
    ldr   r1, =_sdata
    ldr   r2, =_edata
1:  cmp   r1, r2
    bcc   2f
    b     3f
2:  ldr   r3, [r0], #4
    str   r3, [r1], #4
    b     1b

    @ 清零 .bss
3:  ldr   r1, =_sbss
    ldr   r2, =_ebss
    movs  r3, #0
4:  cmp   r1, r2
    bcc   5f
    b     6f
5:  str   r3, [r1], #4
    b     4b

6:  bl    SystemInit
    bl    __libc_init_array
    bl    main
7:  b     7b                      @ main 不应返回
.size Reset_Handler, . - Reset_Handler

@ ==================== Default_Handler ====================
.section .text.Default_Handler
.weak Default_Handler
.type Default_Handler, %function
Default_Handler:
    b     Default_Handler
.size Default_Handler, . - Default_Handler

@ FreeRTOS port 中断别名(指向 ARM_CM3 port.c 实现)
.weak vPortSVCHandler
.weak xPortPendSVHandler
.weak xPortSysTickHandler
