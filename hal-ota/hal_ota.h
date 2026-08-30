#ifndef HAL_OTA_H
#define HAL_OTA_H

#include <stdbool.h>
#include <stdint.h>

// ==========================================
// 硬件抽象层 (HAL) - 定义硬件操作接口
// ==========================================
typedef struct {
    // 底层Flash擦除、写入、读取函数指针
    bool (*flash_erase)(uint32_t addr, uint32_t size);
    bool (*flash_write)(uint32_t addr, const uint8_t *data, uint32_t len);
    void (*flash_read)(uint32_t addr, uint8_t *data, uint32_t len);
    // 系统复位函数
    void (*system_reset)(void);
} hal_ota_t;

#endif // HAL_OTA_H
