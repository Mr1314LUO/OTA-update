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
#include "fsm/ota_fsm.h"


// 声明外部HAL实例
extern hal_ota_t hal_ota_instance;
// 定义全局HAL指针，用于在状态机中访问HAL接口
static hal_ota_t *g_hal = NULL;

// ==========================================
// RTOS 共享对象
// ==========================================
#define OTA_EVENT_QUEUE_LEN     8
#define DOWNLOAD_QUEUE_LEN      2

static QueueHandle_t xEventQueue;      // OTA 事件队列（ota_event_t）
static QueueHandle_t xDownloadQueue;   // 下载请求队列（uint32_t：升级包大小）
static TaskHandle_t  xConfirmTaskHandle;

// printf.h 跨任务串行化用的互斥锁（POSIX 移植层要求）
SemaphoreHandle_t g_stdio_mutex = NULL;

// 状态机上下文：ota_task 拥有；download_task 仅更新进度字段（ota_task 此时阻塞在事件队列上）
static ota_context_t g_ota_ctx = {0};

#endif // OTA_MAIN_H