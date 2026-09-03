#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include "update-table/firmware_update.h"

// 固件模块数组（extern 声明在 firmware_update.h 中，定义在 module_manager.c 中）
// 模块数量统计
static int g_module_count = 0;
// 在链接脚本中定义内存区域，用于存放升级清单
extern uint8_t g_update_manifest[];

// 主机模拟用演示清单：真实设备上由服务器下发
static const char *demo_manifest =
    "# OTA upgrade manifest\n"
    "module=firmware\n"
    "version=V1.0\n"
    "target_version=V2.0\n"
    "required=1\n";

// 解析升级清单，填充模块信息表（g_modules）
bool module_manager_parse_manifest(const uint8_t *manifest_data, uint32_t len);

// 按模块名查询模块信息，未找到返回 NULL
firmware_module_t* module_manager_get_module(const char *name);

// 判断是否有模块需要更新
bool module_manager_is_update_available(void);

#endif // MODULE_MANAGER_H
