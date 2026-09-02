#include <stdio.h>
#include <unistd.h>
#include "hal/hal_ota.h"
// #include "stm32f1xx_hal.h" // 假设使用STM32

void HAL_Init(void){
    // ... 初始化HAL库 ...

}
// 实现具体的Flash操作函数
static bool stm32_flash_erase(uint32_t addr, uint32_t size) {
    // 主机端桩实现：不访问真实硬件
    (void)addr;
    (void)size;
    // ... 调用HAL_FLASH_Unlock()，按页擦除等 ...
    return true;
}
static bool stm32_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    // 主机端桩实现：模拟 Flash 写入速度（约 1 MB/s）
    (void)addr;
    (void)data;
    // 模拟写入耗时：每字节 1 微秒 ≈ 1 MB/s 吞吐量
    usleep(len*2);
    // ... 调用HAL_FLASH_Program()，按字/半字编程 ...
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