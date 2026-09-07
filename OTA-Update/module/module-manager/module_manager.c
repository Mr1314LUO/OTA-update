// module_manager.c
#include "module_manager.h"

// 固件模块数组定义（声明在 firmware_update.h 中）
firmware_module_t g_modules[MAX_MODULES];

// 手写整数解析(base 0:支持十进制与 0x 十六进制),替代 strtoul。
// 清单中 size/required 字段均为简单数值,无需 newlib strtoul(可能拉入 locale)。
static uint32_t parse_uint(const char *s) {
    uint32_t base = 10u, v = 0u;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16u;
        s += 2;
    }
    while (*s != '\0') {
        int d = -1;
        if (*s >= '0' && *s <= '9') {
            d = *s - '0';
        } else if (base == 16u && *s >= 'a' && *s <= 'f') {
            d = *s - 'a' + 10;
        } else if (base == 16u && *s >= 'A' && *s <= 'F') {
            d = *s - 'A' + 10;
        }
        if (d < 0 || (uint32_t)d >= base) {
            break;
        }
        v = v * base + (uint32_t)d;
        s++;
    }
    return v;
}

//
bool module_manager_parse_manifest(const uint8_t *data, uint32_t len) {
    // 行式清单格式（'#' 开头为注释），每个模块以 "module=<名称>" 开始：
    //   module=firmware
    //   version=V1.0
    //   target_version=V1.1
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

        // 解析 key=value(手写,替代 sscanf:sscanf 会拉入 newlib dtoa/localeconv ~数 KB)
        char key[32], value[80];
        char *eq = strchr(s, '=');
        if (eq == NULL) {
            continue;   // 无 '=',跳过
        }
        size_t klen = (size_t)(eq - s);
        if (klen == 0 || klen >= sizeof(key)) {
            continue;   // key 为空或过长(与 sscanf %31 截断+失败语义一致)
        }
        memcpy(key, s, klen);
        key[klen] = '\0';
        const char *vp = eq + 1;
        size_t vlen = 0;
        while (vp[vlen] != '\0' && vp[vlen] != '\r' && vp[vlen] != '\n' &&
               vlen < sizeof(value) - 1) {
            vlen++;
        }
        memcpy(value, vp, vlen);
        value[vlen] = '\0';

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
            // 版本字符串原样保存，如 "V1.0"
            strncpy(cur->version, value, MODULE_VERSION_STR_LEN - 1);
            cur->version[MODULE_VERSION_STR_LEN - 1] = '\0';
        } else if (strcmp(key, "target_version") == 0) {
            // 目标版本字符串原样保存，如 "V1.1"
            strncpy(cur->target_version, value, MODULE_VERSION_STR_LEN - 1);
            cur->target_version[MODULE_VERSION_STR_LEN - 1] = '\0';
        } else if (strcmp(key, "size") == 0) {
            cur->size = parse_uint(value);
        } else if (strcmp(key, "required") == 0) {
            cur->update_required = (parse_uint(value) != 0u);
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
