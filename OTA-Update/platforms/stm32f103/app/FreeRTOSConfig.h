// FreeRTOSConfig.h —— STM32F103C8T6 (Cortex-M3) MCU 调优版
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "stm32f103xb.h"  // 提供 __NVIC_PRIO_BITS,或下面手动定义

#ifndef __NVIC_PRIO_BITS
    #define __NVIC_PRIO_BITS    4   // Cortex-M3 优先级寄存器位数
#endif

// ---------- 调度策略 ----------
#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  1   // CM3 有 CLZ,可优化
#define configUSE_TICKLESS_IDLE                  0
#define configTICK_TYPE_WIDTH_IN_BITS            TICK_TYPE_WIDTH_32_BITS
#define configTICK_RATE_HZ                       ( ( TickType_t ) 1000 )
#define configCPU_CLOCK_HZ                       ( ( unsigned long ) 72000000UL )  // HSE 8MHz × PLL9
#define configMAX_PRIORITIES                     5   // 够 OTA 用,省 TCB 数组
#define configMINIMAL_STACK_SIZE                  ( ( configSTACK_DEPTH_TYPE ) 128 )
#define configMAX_TASK_NAME_LEN                   12
#define configIDLE_SHOULD_YIELD                  1
#define configSTACK_DEPTH_TYPE                    uint16_t  // 省 RAM(M3 上 uint32_t→uint16_t 节省)

// ---------- 任务通知 / 同步原语 ----------
#define configUSE_TASK_NOTIFICATIONS             1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    1
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              0   // OTA 不用递归,省 Flash
#define configUSE_COUNTING_SEMAPHORES            0
#define configUSE_QUEUE_SETS                     0
#define configQUEUE_REGISTRY_SIZE                0

// ---------- 内存管理(heap_4) ----------
#define configSUPPORT_STATIC_ALLOCATION          0
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configTOTAL_HEAP_SIZE                    ( ( size_t ) ( 3 * 1024 ) )  // 3KB(MCU)
#define configAPPLICATION_ALLOCATED_HEAP         0
#define configUSE_NEWLIB_REENTRANT               0
#define configENABLE_BACKWARD_COMPATIBILITY      0

// ---------- 软件定时器 ----------
// ARM_CM3 port 不需要 timers,关闭省 Flash + RAM
#define configUSE_TIMERS                         0
#define configTIMER_TASK_PRIORITY                0
#define configTIMER_QUEUE_LENGTH                 0
#define configTIMER_TASK_STACK_DEPTH             0

// ---------- 钩子 ----------
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configUSE_MALLOC_FAILED_HOOK             1
#define configCHECK_FOR_STACK_OVERFLOW           2   // 启用方法 2(更严格)

// ---------- 内核 API 裁剪 ----------
#define INCLUDE_vTaskPrioritySet                 0
#define INCLUDE_uxTaskPriorityGet                0
#define INCLUDE_vTaskDelete                      1
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_vTaskDelayUntil                  1
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_xTaskGetCurrentTaskHandle        1
#define INCLUDE_uxTaskGetStackHighWaterMark      1
#define INCLUDE_xTaskGetIdleTaskHandle           0
#define INCLUDE_eTaskGetState                    0
#define INCLUDE_xTaskAbortDelay                  0
#define INCLUDE_xTaskGetHandle                   0
#define INCLUDE_xTaskResumeFromISR               0
#define INCLUDE_xTimerPendFunctionCall           0

// ---------- 中断优先级配置(ARM_CM3 port 关键) ----------
// Cortex-M3 优先级寄存器高 4 位有效,数值越大优先级越低
#define configKERNEL_INTERRUPT_PRIORITY         ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5
#define configPRIO_BITS                            __NVIC_PRIO_BITS

// ---------- 断言与钩子实现 ----------
// 实现位于 OTA-Update/app/ota_main.c 或本工程 main.c
void vAssertCalled( const char *file, int line );
#define configASSERT( x )    if( ( x ) == 0 ) vAssertCalled( __FILE__, __LINE__ )

void vApplicationMallocFailedHook( void );

#endif // FREERTOS_CONFIG_H
