// module_manager.c
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include "update-table/firmware_update.h"
#include "module_manager.h"

// 固件模块数组定义（声明在 firmware_update.h 中）
firmware_module_t g_modules[MAX_MODULES];
static int g_module_count = 0;

//
bool module_manager_parse_manifest(const uint8_t *data, uint32_t len) {
    // 行式清单格式（'#' 开头为注释），每个模块以 "module=<名称>" 开始：
    //   module=firmware
    //   version=1
    //   target_version=2
    //   required=1
    g_module_count = 0;
    if (data == NULL || len == 0) {
        return false;
    }

    firmware_module_t *cur = NULL;
    char line[128];
    uint32_t pos = 0;

    while (pos < len) {
        // 提取一行
        uint32_t end = pos;
        while (end < len && data[end] != '\n') {
            end++;
        }
        uint32_t line_len = end - pos;
        if (line_len >= sizeof(line)) {
            line_len = sizeof(line) - 1;
        }
        memcpy(line, &data[pos], line_len);
        line[line_len] = '\0';
        pos = end + 1;

        // 跳过空行、注释
        char *s = line;
        while (*s == ' ' || *s == '\t') {
            s++;
        }
        if (*s == '\0' || *s == '#' || *s == '\r') {
            continue;
        }

        // 解析 key=value
        char key[32], value[80];
        if (sscanf(s, "%31[^=]=%79[^\r\n]", key, value) != 2) {
            continue;
        }

        if (strcmp(key, "module") == 0) {
            // 新模块条目
            if (g_module_count >= MAX_MODULES) {
                break;
            }
            cur = &g_modules[g_module_count++];
            memset(cur, 0, sizeof(*cur));
            strncpy(cur->name, value, MAX_MODULE_NAME - 1);
        } else if (cur == NULL) {
            // 模块属性出现在 module= 之前，忽略
            continue;
        } else if (strcmp(key, "version") == 0) {
            cur->version = (uint32_t)strtoul(value, NULL, 0);
        } else if (strcmp(key, "target_version") == 0) {
            cur->target_version = (uint32_t)strtoul(value, NULL, 0);
        } else if (strcmp(key, "size") == 0) {
            cur->size = (uint32_t)strtoul(value, NULL, 0);
        } else if (strcmp(key, "required") == 0) {
            cur->update_required = (strtoul(value, NULL, 0) != 0);
        }
        // 其余字段（如 md5 等）暂不处理
    }

    return g_module_count > 0;
}
//
firmware_module_t* module_manager_get_module(const char *name) {
    for (int i = 0; i < g_module_count; i++) {
        if (strcmp(g_modules[i].name, name) == 0) {
            return &g_modules[i];
        }
    }
    return NULL;
}
//
bool module_manager_is_update_available(void) {
    for (int i = 0; i < g_module_count; i++) {
        if (g_modules[i].update_required) {
            return true;
        }
    }
    return false;
}

// 差分升级应用函数 (简化版)[reference:9]
int apply_diff(uint32_t old_base, uint32_t new_base, uint8_t *diff_buf) {
    // ... 解析diff_buf，根据其中的COPY和ADD指令，
    //     从old_base读取数据，写入到new_base ...
    return 0; // 成功
}
