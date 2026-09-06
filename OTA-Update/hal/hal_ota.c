#include <stdio.h>
#include <unistd.h>
#include "hal/hal_ota.h"
// #include "stm32f1xx_hal.h" // 假设使用STM32

// ==========================================
// 延时适配
// FreeRTOS 环境：任务内必须使用 vTaskDelay 让出 CPU（usleep 会阻塞任务线程，
// 且可能被 tick 信号打断导致调度异常）；us 微秒级延时向上取整到毫秒/tick
// ==========================================
#ifdef USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
static inline void host_delay_us(uint32_t us) {
    TickType_t ticks = pdMS_TO_TICKS((us + 999u) / 1000u);
    if (ticks == 0) {
        ticks = 1;      // 短延时至少让出一个 tick
    }
    vTaskDelay(ticks);
}
#else
static inline void host_delay_us(uint32_t us) {
    usleep(us);
}
#endif

void HAL_Init(void){
    // ... 初始化HAL库 ...

}
// 实现具体的Flash操作函数
// 按页擦除（模拟STM32典型页大小 2KB），每页擦除后回调进度
static bool stm32_flash_erase(uint32_t addr, uint32_t size, erase_progress_cb_t progress_cb, const char *module_name) {
    // 主机端桩实现：不访问真实硬件
    (void)addr;

    // STM32 典型Flash页大小 2KB
    const uint32_t page_size = 2 * 1024;
    uint32_t erased = 0;

    while (erased < size) {
        uint32_t chunk = (size - erased < page_size) ? (size - erased) : page_size;
        // 模拟每页擦除耗时
        host_delay_us(chunk * 1);
        erased += chunk;

        // 回调报告进度百分比
        if (progress_cb) {
            uint32_t progress = (erased * 100) / size;
            if (progress > 100) progress = 100;
            progress_cb(progress, module_name);
        }
    }

    return true;
}
static bool stm32_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    // 主机端桩实现：模拟 Flash 写入速度（约 1 MB/s）
    (void)addr;
    (void)data;

    // ... 调用HAL_FLASH_Program()，按字/半字编程 ...
    // 模拟写入耗时：每字节 1 微秒 ≈ 1 MB/s 吞吐量
    host_delay_us(len*2);

    return true;
}
static void stm32_flash_read(uint32_t addr, uint8_t *data, uint32_t len) {
    // 主机端桩实现：不访问真实硬件
    (void)addr;
    (void)data;
    (void)len;
    // ... 直接内存拷贝 ...
}
// 主机端(Host)构建的桩实现；真实 STM32 工程中应链接 HAL 库
void HAL_NVIC_SystemReset(void) {
    // 主机端无法真正复位，仅占位
}

static void stm32_system_reset(void) {
    HAL_NVIC_SystemReset();
}

// 定义并导出HAL接口实例
hal_ota_t hal_ota_instance = {
    .flash_erase = stm32_flash_erase,
    .flash_write = stm32_flash_write,
    .flash_read = stm32_flash_read,
    .system_reset = stm32_system_reset
};