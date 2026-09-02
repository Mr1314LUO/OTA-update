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
typedef struct {
    uint32_t magic;          /* 魔数：0x46575246 ("FRWF") */
    uint32_t version;        /* 固件版本 */
    uint32_t compressed_size;/* 压缩数据大小 */
    uint32_t uncompressed_size; /* 未压缩数据大小 */
    uint32_t crc32;          /* 未压缩数据的 CRC32 */
    uint8_t  reserved[12];   /* 保留字段 */
} FirmwareHeader_t;
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
 * version_str 为版本字符串（简单整数解析，如 "1" "100"）
 * 成功返回 0，失败返回负数 */
int compressed_File(const char *input_path, const char *output_path,
                    const char *version_str);

#endif