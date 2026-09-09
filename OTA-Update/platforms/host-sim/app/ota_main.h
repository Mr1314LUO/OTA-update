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

extern hal_ota_t hal_ota_instance;
extern hal_ota_t *g_hal;
extern firmware_source_t *g_fw_src;

#define OTA_EVENT_QUEUE_LEN 8
#define DOWNLOAD_QUEUE_LEN 2

extern QueueHandle_t xEventQueue;
extern QueueHandle_t xDownloadQueue;
extern TaskHandle_t xConfirmTaskHandle;
extern SemaphoreHandle_t g_stdio_mutex;
extern ota_context_t g_ota_ctx;
extern bool g_ota_skip_initial_check;

#ifndef HOST_SIM
void ota_app_start(void);
#endif

#endif
