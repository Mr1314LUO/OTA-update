// main.c
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include "printf.h"
#include "hal/hal_ota.h"
#include "fsm/ota_fsm.h"

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
    // 初始化硬件
    HAL_Init();

    // 初始化OTA引擎，传入HAL接口
    if (!ota_engine_init(&hal_ota_instance)) {
        LOG_ERROR("OTA engine init failed\n");
        return 1;
    }

    // 初始化状态机上下文，从空闲状态启动
    ota_context_t ota_ctx = {0};
    ota_ctx.state = OTA_STATE_IDLE;

    // 主循环中周期性调用处理函数：
    // IDLE 状态下引擎自动发起检查，后续流程由各动作的执行结果驱动
    while (1) {
        // 处理状态机
        ota_engine_process(&ota_ctx);
        // ... 执行其他任务 ...
        usleep(100 * 1000);     // 主机模拟：降低空转频率（嵌入式可替换为低功耗休眠）
    }
    return 0;
}