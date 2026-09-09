// ota_main.c —— FreeRTOS 多任务入口
//
// 任务架构：
//
//   main() ── 创建队列/任务 ──► vTaskStartScheduler()
//
//   ┌──────────────┐  EVENT_* 队列   ┌────────────────┐
//   │ download_task │ ─────────────► │    ota_task     │──► fsm_handle_event()
//   │ (主机:模拟下载;  │                │ (状态机所有者)   │
//   │  MCU:校验 SPI Flash)│           └────────────────┘
//   └──────────────┘
//   ┌──────────────┐   EVENT_READY_CONFIRM
//   │ confirm_task  │ ─────────────► ▲
//   │ (模拟用户确认) │                │
//   └──────────────┘                │
//         ▲ 请求下载/确认通过平台钩子（ota_platform_*）发起
//
// 双路径编译：
//   - HOST_SIM  : PC 仿真，main() 入口，基于文件系统 + POSIX 移植层
//   - MCU       : mcu/app/main.c 调 ota_app_start()，基于 SPI Flash + ARM_CM3 port
#include "ota_main.h"

// ==========================================
// 全局定义（ota_main.h 中 extern 声明）
// ==========================================
hal_ota_t       *g_hal               = NULL;
firmware_source_t *g_fw_src           = NULL;
QueueHandle_t   xEventQueue          = NULL;
QueueHandle_t   xDownloadQueue       = NULL;
TaskHandle_t    xConfirmTaskHandle   = NULL;
SemaphoreHandle_t g_stdio_mutex      = NULL;
ota_context_t   g_ota_ctx            = {0};

// P7: 升级后首次启动时由 main.c 置 true,跳过自动 OTA 检查
// 防止 App 反复升级同一版本(Bootloader 写 DONE 后固件仍在 SPI Flash)
bool g_ota_skip_initial_check        = false;

// ==========================================
// MCU 路径专用：SPI Flash 驱动 + 固件头校验
// ==========================================
#ifndef HOST_SIM
#include "stm32f1_spi.h"
#include "w25qxx.h"
#include "firmware_source.h"     // g_spiflash_source 实例
#include "lzma/flow-unzip/unzip_stream.h"  // FirmwareHeader_t / FIRMWARE_MAGIC
#endif

/**
 * @brief OTA引擎初始化函数
 * @param hal OTA硬件抽象层结构体指针
 * @return 初始化成功返回true，失败返回false
 */
bool ota_engine_init(hal_ota_t *hal) {
    if (hal == NULL)
    return false;

    g_hal = hal;
    return true;
}

// ==========================================
// 任务实现
// ==========================================

// OTA 任务：状态机所有者，从事件队列取事件驱动状态转移
static void ota_task(void *arg) {
    ota_event_t event;
    (void)arg;

    for (;;)
    {
        // 等待事件队列消息 (portMAX_DELAY: 阻塞等待，直到有消息到达)
        if (xQueueReceive(xEventQueue, &event, portMAX_DELAY) == pdTRUE) {
            // 处理事件
            fsm_handle_event(&g_ota_ctx, event);
        }
    }
}

// 下载任务：
//   - 主机模拟: 分块接收升级包，完成后回送 EVENT_DOWNLOAD_COMPLETE
//   - MCU     : 校验 SPI Flash 中固件镜像就绪（读头校验 magic/size），完成后回送事件
static void download_task(void *arg) {
    uint32_t total_size;
    (void)arg;

    for (;;) {
        if (xQueueReceive(xDownloadQueue, &total_size, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        ota_event_t event;
#ifdef HOST_SIM
        const uint32_t chunk = 4096;    // 模拟分块传输
        uint32_t received = 0;
        while (received < total_size)
        {
            uint32_t to_recv = total_size - received;
            if (to_recv > chunk) {
                to_recv = chunk;
            }
            received += to_recv;

            // 更新上下文进度（ota_task 此刻阻塞在事件队列，无竞争）
            g_ota_ctx.downloaded_size = received;
            g_ota_ctx.progress = (uint8_t)((received * 100) / total_size);
            LOG_RAW("\r🚀🚀🚀 下载进度: %d%% (%u/%u bytes)",
                    g_ota_ctx.progress, received, total_size);

            // 模拟网络传输延迟（真实设备：此处从网络接收缓冲区读取数据）
            vTaskDelay(pdMS_TO_TICKS(2));
        }
        LOG_RAW("\n");
        event = EVENT_DOWNLOAD_COMPLETE;
#else
        // MCU 路径：固件镜像已预存于 SPI Flash，此处校验头部合法性
        FirmwareHeader_t hdr;
        int n = g_spiflash_source.read(g_spiflash_source.ctx, 0,
                                        (uint8_t*)&hdr, sizeof(hdr));
        if (n != (int)sizeof(hdr) || hdr.magic != FIRMWARE_MAGIC) {
            LOG_ERROR("SPI Flash 中固件头非法 (n=%d magic=0x%08X)\n" RESET,
                      n, (unsigned)(hdr.magic));
            event = EVENT_DOWNLOAD_FAILED;
        } else if (hdr.compressed_size == 0u || hdr.compressed_size > total_size) {
            LOG_ERROR("固件 compressed_size=%u 异常 (expected<=%u)\n" RESET,
                      (unsigned)hdr.compressed_size, (unsigned)total_size);
            event = EVENT_DOWNLOAD_FAILED;
        } else {
            LOG_SUCCESS("固件就绪: comp=%u uncomp=%u ver=%s\n",
                        (unsigned)hdr.compressed_size,
                        (unsigned)hdr.uncompressed_size,
                        hdr.version_str);
            g_ota_ctx.downloaded_size = hdr.compressed_size;
            g_ota_ctx.progress = 100;
            event = EVENT_DOWNLOAD_COMPLETE;
        }
#endif
        xQueueSend(xEventQueue, &event, portMAX_DELAY);
    }
}

// 用户确认任务：收到通知后模拟用户思考耗时，随后确认升级
static void confirm_task(void *arg) {
    (void)arg;

    for (;;) {
        // 等待升级确认请求（action_prepare_update 发起）
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        LOG_INFO(" ⏳ 等待用户确认升级...\n");
        vTaskDelay(pdMS_TO_TICKS(1000));    // 模拟用户确认耗时
        LOG_INFO(" 👍 用户已确认升级\n");

        // 通知 ota_task：用户已确认升级
        ota_event_t event = EVENT_READY_CONFIRM;
        // 发送确认事件到事件队列
        xQueueSend(xEventQueue, &event, portMAX_DELAY);
    }
}

// ==========================================
// 内核钩子
// ==========================================
void vAssertCalled(const char *file, int line) {
#ifdef HOST_SIM
    fprintf(stderr, "FATAL: FreeRTOS assert failed: %s:%d\n", file, line);
    abort();
#else
    (void)file; (void)line;
    __asm volatile ("cpsid i" ::: "memory");
    LOG_ERROR("FATAL: FreeRTOS assert failed\n");
    for (;;) { }
#endif
}
// 内存分配失败钩子：当FreeRTOS无法分配内存时调用
void vApplicationMallocFailedHook(void) {
#ifdef HOST_SIM
    fprintf(stderr, "FATAL: FreeRTOS heap exhausted\n");
    abort();
#else
    LOG_ERROR("FATAL: FreeRTOS heap exhausted\n");
    for (;;) { }
#endif
}

// 栈溢出钩子：configCHECK_FOR_STACK_OVERFLOW=2 时由内核调用
#ifndef HOST_SIM
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    LOG_ERROR("FATAL: stack overflow in %s\n", pcTaskName);
    for (;;) { }
}
#endif

// ==========================================
// 平台异步钩子（ota_fsm.c 调用，见 ota_fsm.h）
// ==========================================
/**
 * @brief 转发下载请求到 download_task（总大小放入请求队列）
 * @param ctx OTA上下文结构体指针
 */
void ota_platform_download_request(ota_context_t *ctx) {
    uint32_t total_size = ctx->total_size;
    xQueueSend(xDownloadQueue, &total_size, portMAX_DELAY);
}

/**
 * @brief 通知 confirm_task：等待用户确认升级
 * MCU 路径 TODO(P6): 此处可触发写 upgrade_flag=MAGIC_PENDING 到 SPI Flash 元数据区
 */
void ota_platform_request_confirm(void) {
    xTaskNotifyGive(xConfirmTaskHandle);
}

// ==========================================
// PC 仿真入口：初始化 → 创建队列/任务 → 启动调度器
// ==========================================
#ifdef HOST_SIM
int main(void) {
    // 初始化硬件
    HAL_Init();

    // 初始化OTA引擎，传入HAL接口
    if (!ota_engine_init(&hal_ota_instance)) {
        LOG_ERROR("OTA engine init failed\n");
        return 1;
    }

    // 初始化状态机上下文，从空闲状态启动
    g_ota_ctx.state = OTA_STATE_IDLE;

    // printf 串行化互斥锁（必须在启动调度器前创建）
    g_stdio_mutex = xSemaphoreCreateMutex();    // 串行化互斥锁，确保 printf 不被中断
    configASSERT(g_stdio_mutex != NULL);    // 确保互斥锁创建成功( 断言检查)

    // 创建队列
    xEventQueue = xQueueCreate(OTA_EVENT_QUEUE_LEN, sizeof(ota_event_t));    // 事件队列，用于 ota_task 与 download_task 通信
    xDownloadQueue = xQueueCreate(DOWNLOAD_QUEUE_LEN, sizeof(uint32_t));    // 下载队列，用于 download_task 与 ota_task 通信
    configASSERT(xEventQueue != NULL);
    configASSERT(xDownloadQueue != NULL);

    // 创建任务（栈深度单位为 StackType_t；POSIX 移植层实际使用 pthread 默认栈）
    // 优先级：ota > download/confirm —— 保证状态机动作执行期间不被中断
    BaseType_t ok = pdPASS;    // 任务创建成功标志位
    ok &= xTaskCreate(ota_task,      "ota",      4096, NULL, 3, NULL);
    ok &= xTaskCreate(download_task, "download", 4096, NULL, 2, NULL);
    ok &= xTaskCreate(confirm_task,  "confirm",  4096, NULL, 2, &xConfirmTaskHandle);
    configASSERT(ok == pdPASS);    // 确保所有任务创建成功( 断言检查)

    // IDLE 状态下发起首次更新检查（真实设备可由云端推送/定时器触发）
    ota_event_t event = EVENT_START_CHECK;  // 触发更新检查事件
    xQueueSend(xEventQueue, &event, 0);    // 发送事件到事件队列

    // 启动调度器（POSIX 移植层：main 线程随后阻塞在 sigwait，不再返回）
    vTaskStartScheduler();

    // 不可达：调度器不返回时保险驻留
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));    // 1s 空闲循环，保持系统运行
    }

    return 0;
}

#else  /* !HOST_SIM —— MCU 路径 */

// ==========================================
// MCU OTA 启动入口：由 mcu/app/main.c 在完成硬件初始化后调用
// 创建队列/任务并发起首次更新检查；调度器由调用方启动
// ==========================================
void ota_app_start(void) {
    // 固件源指向 SPI Flash 后端实例（drivers/firmware_source_spiflash.c 中定义）
    g_fw_src = &g_spiflash_source;

    // OTA 引擎初始化
    if (!ota_engine_init(&hal_ota_instance)) {
        LOG_ERROR("OTA engine init failed\n");
        return;
    }

    // 状态机上下文初始化为 IDLE
    g_ota_ctx.state = OTA_STATE_IDLE;

    // 创建事件/下载队列
    xEventQueue = xQueueCreate(OTA_EVENT_QUEUE_LEN, sizeof(ota_event_t));
    xDownloadQueue = xQueueCreate(DOWNLOAD_QUEUE_LEN, sizeof(uint32_t));
    configASSERT(xEventQueue != NULL);
    configASSERT(xDownloadQueue != NULL);

    // 创建任务（MCU 栈深缩小：ota 0.5K / download 0.4K / confirm 0.25K）
    BaseType_t ok = pdPASS;
    ok &= xTaskCreate(ota_task,      "ota",      512, NULL, 3, NULL);
    ok &= xTaskCreate(download_task, "download", 384, NULL, 2, NULL);
    ok &= xTaskCreate(confirm_task,  "confirm",  256, NULL, 2, &xConfirmTaskHandle);
    configASSERT(ok == pdPASS);

    // 发起首次更新检查(升级后首次启动时跳过,避免反复升级同一版本)
    if (!g_ota_skip_initial_check) {
        ota_event_t event = EVENT_START_CHECK;
        xQueueSend(xEventQueue, &event, 0);
    } else {
        LOG_INFO("跳过自动 OTA 检查(刚完成升级)\n");
    }
}
#endif /* HOST_SIM */
