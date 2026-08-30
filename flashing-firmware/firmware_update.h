#ifndef FIRMWARE_UPDATE_H
#define FIRMWARE_UPDATE_H

#include <stdint.h>
#include <stdbool.h>
#include "ota_framework.h"
#include "hal-ota/hal_ota.h"

// 固件升级目录
#define UPDATE_F_DIR "./Update-Firmware"
// 最大固件名称长度
#define MAX_MODULE_NAME 16
// 最大固件数量
#define MAX_MODULES     8
// 固件分区表大小
#define FIRMWARE_PARTITION_TABLE_SIZE (sizeof(firmware_partition_table) / sizeof(firmware_partition_table[0]))
// 固件升级缓冲区大小
#define FIRMWARE_UPDATE_BUF_SIZE (4 * 1024)

// 外部HAL实例声明
extern hal_ota_t hal_ota_instance;

// 固件模块元数据结构体
typedef struct {
    char name[MAX_MODULE_NAME];   // 模块名，如 "bootloader", "app", "wifi_fw"
    uint32_t version;             // 当前版本号
    uint32_t target_version;      // 目标版本号
    uint32_t flash_start_addr;    // 在Flash中的起始地址
    uint32_t size;                // 模块大小
    bool update_required;         // 是否需要更新
    uint8_t hash[32];             // 固件哈希值
} firmware_module_t;

// 固件模块数组（在 module_manager.c 中定义）
extern firmware_module_t g_modules[MAX_MODULES];

// 固件模块分区表
// 模块名称，固件文件名，Flash起始地址、最大允许大小，升级优先级
typedef struct {
    const char *module_name;      // 模块名称
    const char *firmware_file;    // 固件文件名
    uint32_t flash_start_addr;    // Flash起始地址
    uint32_t max_size;            // 最大允许大小
    uint8_t priority;             // 升级优先级(数字越小越先升级)
} firmware_partition_entry_t;
firmware_module_t* module_manager_get_module(const char *name);

// 固件升级主函数
int firmware_update(void);


#endif
