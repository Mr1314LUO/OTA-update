#ifndef OTA_MAIN_H
#define OTA_MAIN_H

#include <stddef.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "printf.h"
#include "hal/hal_ota.h"
#include "hal/firmware_source.h"
#include "fsm/ota_fsm.h"


// 声明外部HAL实例
extern hal_ota_t hal_ota_instance;
// 全局HAL指针，用于在状态机中访问HAL接口（定义在 ota_main.c）
extern hal_ota_t *g_hal;

// 固件源全局指针（MCU 路径用）
// - PC 仿真（HOST_SIM）: 保持 NULL,FSM 走 stat/file_md5 路径
// - MCU: main 启动时指向 &g_spiflash_source（SPI Flash 后端实例）
extern firmware_source_t *g_fw_src;

// ==========================================
// RTOS 共享对象（定义在 ota_main.c，跨任务共享）
// ==========================================
#define OTA_EVENT_QUEUE_LEN     8
#define DOWNLOAD_QUEUE_LEN      2

extern QueueHandle_t xEventQueue;        // OTA 事件队列（ota_event_t）
extern QueueHandle_t xDownloadQueue;    // 下载请求队列（uint32_t：升级包大小）
extern TaskHandle_t  xConfirmTaskHandle;

// printf.h 跨任务串行化用的互斥锁（仅 POSIX 移植层需要；MCU 用 printf_lite 不需要）
extern SemaphoreHandle_t g_stdio_mutex;

// 状态机上下文：ota_task 拥有；download_task 仅更新进度字段（ota_task 此时阻塞在事件队列上）
extern ota_context_t g_ota_ctx;

// P7: 升级后首次启动标志(由 mcu/app/main.c 在检测到 DONE 标志时置 true)
// ota_app_start() 据此跳过自动 EVENT_START_CHECK,避免反复升级同一版本
extern bool g_ota_skip_initial_check;

// ==========================================
// MCU 入口：由 mcu/app/main.c 调用，创建 OTA 任务并发起首次检查
// 硬件初始化(SPI Flash / UART / RCC)由调用方完成
// ==========================================
#ifndef HOST_SIM
void ota_app_start(void);
#endif

#endif // OTA_MAIN_H