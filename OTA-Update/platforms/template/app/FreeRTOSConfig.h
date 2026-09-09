// FreeRTOSConfig.h —— 平台 FreeRTOS 配置（模板，Cortex-M 示例）
//
// TODO: 按芯片修改 configCPU_CLOCK_HZ、configPRIO_BITS、configTOTAL_HEAP_SIZE。
// 非 Cortex-M 架构（RISC-V 等）请参考 FreeRTOS 官方对应 port 的 demo 配置。
// 参考：platforms/stm32f103/app/FreeRTOSConfig.h
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

// TODO: 引入芯片头文件以提供优先级位数（或手动 #define __NVIC_PRIO_BITS）
// #include "your_chip_header.h"
#ifndef __NVIC_PRIO_BITS
    #define __NVIC_PRIO_BITS    4   // TODO: Cortex-M 优先级位数（M0/M3/M4 通常 4，M7 多为 4）
#endif

// ---------- 调度策略 ----------
#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  1   // 有 CLZ 指令的核(Cortex-M3+)可置 1
#define configUSE_TICKLESS_IDLE                  0
#define configTICK_TYPE_WIDTH_IN_BITS            TICK_TYPE_WIDTH_32_BITS
#define configTICK_RATE_HZ                       ((TickType_t)1000)
#define configCPU_CLOCK_HZ                       72000000UL   // TODO: 系统主频(Hz)
#define configMAX_PRIORITIES                     5
#define configMINIMAL_STACK_SIZE                 ((configSTACK_DEPTH_TYPE)128)
#define configMAX_TASK_NAME_LEN                  12
#define configIDLE_SHOULD_YIELD                  1
#define configSTACK_DEPTH_TYPE                   uint16_t

// ---------- 同步原语 ----------
#define configUSE_TASK_NOTIFICATIONS             1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    1
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              0
#define configUSE_COUNTING_SEMAPHORES            0
#define configUSE_QUEUE_SETS                      0
#define configQUEUE_REGISTRY_SIZE                0

// ---------- 内存管理(heap_4) ----------
#define configSUPPORT_STATIC_ALLOCATION          0
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configTOTAL_HEAP_SIZE                    ((size_t)(3 * 1024))   // TODO: 按 RAM 调整
#define configAPPLICATION_ALLOCATED_HEAP         0
#define configUSE_NEWLIB_REENTRANT               0
#define configENABLE_BACKWARD_COMPATIBILITY      0

// ---------- 软件定时器 ----------
// 注意：configUSE_TIMERS=0 时 os/freertos/timers.c 不要加入 App 构建
#define configUSE_TIMERS                         0
#define configTIMER_TASK_PRIORITY                0
#define configTIMER_QUEUE_LENGTH                 0
#define configTIMER_TASK_STACK_DEPTH             0

// ---------- 钩子 ----------
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configUSE_MALLOC_FAILED_HOOK             1
#define configCHECK_FOR_STACK_OVERFLOW           2

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

// ---------- 中断优先级配置(Cortex-M) ----------
#define configKERNEL_INTERRUPT_PRIORITY          (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY     (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5
#define configPRIO_BITS                           __NVIC_PRIO_BITS

// ---------- 断言与钩子实现 ----------
// vAssertCalled / vApplicationMallocFailedHook / vApplicationStackOverflowHook
// 由 lib/app/ota_main.c 的 MCU 路径提供，无需平台实现。
void vAssertCalled(const char *file, int line);
#define configASSERT(x)    if ((x) == 0) vAssertCalled(__FILE__, __LINE__)
void vApplicationMallocFailedHook(void);

#endif // FREERTOS_CONFIG_H
