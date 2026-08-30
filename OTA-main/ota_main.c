// main.c
#include "incremental-update/ota_framework.h"
#include "hal-ota/hal_ota.h"
#include "fsm-table-driven/table_driven_fsm.h"

// 声明外部HAL实例
extern hal_ota_t hal_ota_instance;
// 定义全局HAL指针，用于在状态机中访问HAL接口
static hal_ota_t *g_hal = NULL;

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

int main(void) {
    // 1. 初始化硬件
    HAL_Init();
    // ...

    // 2. 初始化OTA引擎，传入HAL接口
    if (!ota_engine_init(&hal_ota_instance)) {
        // 处理错误
    }

    ota_context_t ota_ctx = {0};    // 初始化状态机上下文
    // ota_ctx.state = OTA_STATE_IDLE; // 初始化状态为空闲
    ota_ctx.state = OTA_STATE_READY; // 初始化状态为准备就绪

    ota_engine_process(&ota_ctx);

    // // 3. 在主循环中周期性调用处理函数
    // while (1) {
    //     ota_engine_process(&ota_ctx);
    //     // ... 执行其他任务 ...
    // }
}