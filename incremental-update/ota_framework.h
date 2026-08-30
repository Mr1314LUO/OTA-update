#ifndef OTA_FRAMEWORK_H
#define OTA_FRAMEWORK_H

#include <stdint.h>
#include <stdbool.h>
#include "fsm-table-driven/table_driven_fsm.h"




// 模块管理函数
bool module_manager_parse_manifest(const uint8_t *manifest_data, uint32_t len);
bool module_manager_is_update_available(void);

#endif // OTA_FRAMEWORK_H