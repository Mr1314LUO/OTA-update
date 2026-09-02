#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#include "update/firmware_update.h"

// 解析升级清单，填充模块信息表（g_modules）
bool module_manager_parse_manifest(const uint8_t *manifest_data, uint32_t len);

// 按模块名查询模块信息，未找到返回 NULL
firmware_module_t* module_manager_get_module(const char *name);

// 判断是否有模块需要更新
bool module_manager_is_update_available(void);

#endif // MODULE_MANAGER_H
