#ifndef __UNZIP_STREAM_H__
#define __UNZIP_STREAM_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "LzmaDec.h"
#include "7zCrc.h"
#include "7zAlloc.h"
#include "Alloc.h"

/* ================================================================
 * 固件头部定义（与 firmware_create.c / firmware_update.c 一致）
 * 与 lzma/unzip/unzip.h、lzma/zip/zip.h 共享：统一防护宏
 * ================================================================ */
#ifndef __FIRMWARE_HEADER_T_DEFINED__
#define __FIRMWARE_HEADER_T_DEFINED__

/* 版本字符串最大长度（含 '\0'），如 "V1.1" */
#define FW_VERSION_STR_LEN 12

typedef struct {
    uint32_t magic;              /* 魔数：0x46575246 ("FRWF") */
    uint32_t version;            /* 固件版本（数值编码：主版本<<16 | 次版本） */
    uint32_t compressed_size;    /* 压缩数据大小（含 LZMA 属性头） */
    uint32_t uncompressed_size;  /* 未压缩数据大小 */
    uint32_t crc32;              /* 未压缩数据的 CRC32 */
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

/* 流式解压缓冲区（可按嵌入式内存调整） */
#define INPUT_BUFFER_SIZE  (4 * 1024)
#define OUTPUT_BUFFER_SIZE (4 * 1024)

/* 全局内存分配器（LZMA SDK 使用）
 * 注：新版 SDK（lzma2602）中 SzAlloc/SzFree 已改为 Alloc.c 内部 static，
 *     公开接口是全局分配器实例 g_Alloc（见 Alloc.h）。
 *     这里用宏转发保持 g_FirmwareAlloc 命名不变，&g_FirmwareAlloc
 *     展开为 &g_Alloc，与 LzmaDec_Allocate 等的 ISzAllocPtr 参数匹配 */
#define g_FirmwareAlloc g_Alloc

/* 解压总字节数（用于统计）
 * 注：此头会被多个编译单元包含，变量加 unused 属性避免
 *     "defined but not used" 警告（真正使用方是 unzip_stream.c） */
static uint64_t  g_total_uncompressed __attribute__((unused)) = 0;

/* 进度显示 */
static int g_last_percent __attribute__((unused)) = -1;

/* 解压输出回调：write_user 为用户上下文，data 为本次解压输出块
 * 返回 0 表示写入成功继续解压，返回非 0 中止解压 */
typedef int (*fw_output_write_fn)(void *write_user, const uint8_t *data, size_t size);

/* 执行固件流式解压：将 firmware_path (.lzma 包) 解压到 output_path
 * 成功返回 0，失败返回负数错误码（实现在 unzip_stream.c） */
int perform_firmware_update(const char *firmware_path, const char *output_path);

/* 真流式解压：解压数据不落盘，逐块写入 write_fn（如直接写 Flash）
 * 内部完成固件头部校验与 CRC32 校验，成功返回 0，失败返回负数错误码
 * out_crc32/out_total 可为 NULL */
int perform_firmware_update_stream(const char *firmware_path,
                                   fw_output_write_fn write_fn, void *write_user,
                                   uint32_t *out_crc32, uint64_t *out_total);

#endif