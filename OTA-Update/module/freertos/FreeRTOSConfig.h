#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

// ==========================================
// FreeRTOS 配置 —— Linux 主机 POSIX 移植层
// 配合 portable/ThirdParty/GCC/Posix 使用：
//   - 每个任务运行在独立 pthread 中（栈参数仅占位，实际为 pthread 默认栈）
//   - tick 由 SIGALRM 模拟，任务切换由 SIGUSR1 驱动
// ==========================================

// ---------- 调度策略 ----------
#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  0
#define configUSE_TICKLESS_IDLE                  0
#define configTICK_TYPE_WIDTH_IN_BITS            TICK_TYPE_WIDTH_32_BITS
#define configTICK_RATE_HZ                       ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES                     8
// POSIX 移植层实际使用 pthread 默认栈，此值仅需容纳移植层的线程控制结构
#define configMINIMAL_STACK_SIZE                 ( ( configSTACK_DEPTH_TYPE ) 512 )
#define configMAX_TASK_NAME_LEN                  24
#define configIDLE_SHOULD_YIELD                  1
#define configSTACK_DEPTH_TYPE                   uint32_t

// ---------- 任务通知 / 同步原语 ----------
#define configUSE_TASK_NOTIFICATIONS             1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    1
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              1
#define configUSE_COUNTING_SEMAPHORES            1
#define configUSE_QUEUE_SETS                     0
#define configQUEUE_REGISTRY_SIZE                0

// ---------- 内存管理（heap_4） ----------
#define configSUPPORT_STATIC_ALLOCATION          0
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configTOTAL_HEAP_SIZE                    ( ( size_t ) ( 1024u * 1024u ) )
#define configAPPLICATION_ALLOCATED_HEAP         0
#define configUSE_NEWLIB_REENTRANT               0
#define configENABLE_BACKWARD_COMPATIBILITY      0

// ---------- 软件定时器 ----------
// POSIX 移植层的 port.c 引用了 timers.h，这里启用定时器服务任务
#define configUSE_TIMERS                         1
#define configTIMER_TASK_PRIORITY                ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                 8
#define configTIMER_TASK_STACK_DEPTH             512

// ---------- 钩子 ----------
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configUSE_MALLOC_FAILED_HOOK             1
#define configCHECK_FOR_STACK_OVERFLOW           0

// ---------- 内核 API 裁剪 ----------
#define INCLUDE_vTaskPrioritySet                 0
#define INCLUDE_uxTaskPriorityGet                0
#define INCLUDE_vTaskDelete                      1
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_vTaskDelayUntil                  1
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_xTaskGetCurrentTaskHandle        1
#define INCLUDE_uxTaskGetStackHighWaterMark      0
#define INCLUDE_xTaskGetIdleTaskHandle           0
#define INCLUDE_eTaskGetState                    0
#define INCLUDE_xTaskAbortDelay                  0
#define INCLUDE_xTaskGetHandle                   0
#define INCLUDE_xTaskResumeFromISR               0
#define INCLUDE_xTimerPendFunctionCall           0

// ---------- 断言与钩子实现 ----------
// 实现位于 app/ota_main.c
void vAssertCalled( const char * file, int line );
#define configASSERT( x )                        if ( ( x ) == 0 ) vAssertCalled( __FILE__, __LINE__ )

void vApplicationMallocFailedHook( void );

#endif // FREERTOS_CONFIG_H
