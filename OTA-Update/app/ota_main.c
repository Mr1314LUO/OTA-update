// ota_main.c —— FreeRTOS 多任务入口
//
// 任务架构（FreeRTOS POSIX 移植层，Linux 主机模拟）：
//
//   main() ── 创建队列/任务 ──► vTaskStartScheduler()
//
//   ┌──────────────┐  EVENT_* 队列   ┌────────────────┐
//   │ download_task │ ─────────────► │    ota_task     │──► fsm_handle_event()
//   │ (模拟网络下载) │                │ (状态机所有者)   │
//   └──────────────┘                 └────────────────┘
//   ┌──────────────┐   EVENT_READY_CONFIRM
//   │ confirm_task  │ ─────────────► ▲
//   │ (模拟用户确认) │                │
//   └──────────────┘                │
//         ▲ 请求下载/确认通过平台钩子（ota_platform_*）发起
//
#include "ota_main.h"

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

// 下载任务：模拟分块接收升级包，完成后回送 EVENT_DOWNLOAD_COMPLETE
static void download_task(void *arg) {
    uint32_t total_size;
    const uint32_t chunk = 4096;    // 模拟分块传输
    (void)arg;

    for (;;) {
        if (xQueueReceive(xDownloadQueue, &total_size, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /**************** 模拟分块接收升级包 ****************/
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

        ota_event_t event = EVENT_DOWNLOAD_COMPLETE;
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
    fprintf(stderr, "FATAL: FreeRTOS assert failed: %s:%d\n", file, line);
    abort();
}
// 内存分配失败钩子：当FreeRTOS无法分配内存时调用
void vApplicationMallocFailedHook(void) {
    fprintf(stderr, "FATAL: FreeRTOS heap exhausted\n");
    abort();
}

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
 */
void ota_platform_request_confirm(void) {
    xTaskNotifyGive(xConfirmTaskHandle);
}

// ==========================================
// 入口：初始化 → 创建队列/任务 → 启动调度器
// ==========================================
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
