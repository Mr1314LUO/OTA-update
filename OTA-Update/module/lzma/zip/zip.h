#ifndef ZIP_H
#define ZIP_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "7zCrc.h"
#include "7zAlloc.h"
#include "Alloc.h"

/* 固件头部结构（与 firmware_update.c 一致）
 * 多头文件共享：使用统一防护宏避免重复定义 */
#ifndef __FIRMWARE_HEADER_T_DEFINED__
#define __FIRMWARE_HEADER_T_DEFINED__

/* 版本字符串最大长度（含 '\0'），如 "V1.1" */
#define FW_VERSION_STR_LEN 12

typedef struct {
    uint32_t magic;          /* 魔数：0x46575246 ("FRWF") */
    uint32_t version;        /* 固件版本（数值编码：主版本<<16 | 次版本） */
    uint32_t compressed_size;/* 压缩数据大小 */
    uint32_t uncompressed_size; /* 未压缩数据大小 */
    uint32_t crc32;          /* 未压缩数据的 CRC32 */
    char     version_str[FW_VERSION_STR_LEN]; /* 版本字符串，如 "V1.1"（复用原 reserved 空间，头部总长不变） */
} FirmwareHeader_t;

/* 将 "V1.2" / "1.2" 形式的版本字符串编码为数值版本号：(主版本 << 16) | 次版本 */
static inline uint32_t fw_version_encode(const char *str)
{
    uint32_t major = 0, minor = 0;

    if (str == NULL) {
        return 0;
    }
    /* 跳过 'V'/'v' 等非数字前缀 */
    while (*str != '\0' && (*str < '0' || *str > '9')) {
        str++;
    }
    while (*str >= '0' && *str <= '9') {
        major = major * 10u + (uint32_t)(*str - '0');
        str++;
    }
    if (*str == '.') {
        str++;
        while (*str >= '0' && *str <= '9') {
            minor = minor * 10u + (uint32_t)(*str - '0');
            str++;
        }
    }
    return ((major & 0xFFFFu) << 16) | (minor & 0xFFFFu);
}

/* 将数值版本号格式化为 "V主.次" 字符串写入 buf */
static inline void fw_version_to_str(uint32_t version, char *buf, size_t len)
{
    if (buf == NULL || len == 0) {
        return;
    }
    snprintf(buf, len, "V%u.%u",
             (unsigned)((version >> 16) & 0xFFFFu),
             (unsigned)(version & 0xFFFFu));
}

/* 获取固件头中用于展示的版本字符串：优先 version_str，为空时由数值版本号生成 */
static inline const char *fw_version_display(const FirmwareHeader_t *hdr,
                                             char *buf, size_t len)
{
    if (hdr != NULL && hdr->version_str[0] != '\0') {
        return hdr->version_str;
    }
    fw_version_to_str(hdr != NULL ? hdr->version : 0, buf, len);
    return buf;
}
#endif /* __FIRMWARE_HEADER_T_DEFINED__ */

#ifndef FIRMWARE_MAGIC
#define FIRMWARE_MAGIC 0x46575246
#endif

/* 全局内存分配器
 * 新版 SDK 中 SzAlloc/SzFree 为内部 static，公开接口是 g_Alloc (Alloc.h)
 * 用宏转发，与 unzip_stream.h 保持一致，避免冲突 */
#ifndef g_FirmwareAlloc
#define g_FirmwareAlloc g_Alloc
#endif

/* 压缩固件包：将 input_path 原始文件压缩为 output_path (.lzma 包)
 * version_str 为版本字符串，原样写入固件头（如 "V1.1"；NULL/空串默认 "V1.0"）
 * 成功返回 0，失败返回负数 */
int compressed_File(const char *input_path, const char *output_path,
                    const char *version_str);

#endif