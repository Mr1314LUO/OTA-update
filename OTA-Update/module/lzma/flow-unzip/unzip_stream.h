#ifndef __UNZIP_STREAM_H__
#define __UNZIP_STREAM_H__

#include <stdint.h>
#include <string.h>

#include "LzmaDec.h"
#include "7zCrc.h"
#include "firmware_source.h"  /* firmware_source_t:HOST_SIM 与 MCU 通用 */

#ifdef HOST_SIM
/* PC 仿真:依赖 malloc + 文件,引入 Alloc.h/7zAlloc.h */
#include <stdio.h>
#include <stdlib.h>
#include "7zAlloc.h"
#include "Alloc.h"
#else
/* MCU:用静态 arena 分配器替 malloc,printf/snprintf 重定向到 printf_lite */
#include "lzma_alloc.h"
#include "printf.h"
#endif

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
#ifndef INPUT_BUFFER_SIZE
#define INPUT_BUFFER_SIZE  (4 * 1024)
#endif
#ifndef OUTPUT_BUFFER_SIZE
#define OUTPUT_BUFFER_SIZE (4 * 1024)
#endif

/* 全局内存分配器(LZMA SDK 使用)
 * - HOST_SIM:g_Alloc(Alloc.c,基于 malloc)
 * - MCU:g_McuAlloc(lzma_alloc.c,静态 arena bump allocator)
 * 用宏统一命名,传 &g_FirmwareAlloc 给 LzmaDec_Allocate 等 */
#ifdef HOST_SIM
#define g_FirmwareAlloc g_Alloc
#else
#define g_FirmwareAlloc g_McuAlloc
#endif

/* 解压总字节数（用于统计）
 * 注：此头会被多个编译单元包含，变量加 unused 属性避免
 *     "defined but not used" 警告（真正使用方是 unzip_stream.c） */
static uint64_t  g_total_uncompressed __attribute__((unused)) = 0;

/* 进度显示 */
static int g_last_percent __attribute__((unused)) = -1;

/* 解压输出回调：write_user 为用户上下文，data 为本次解压输出块
 * 返回 0 表示写入成功继续解压，返回非 0 中止解压 */
typedef int (*fw_output_write_fn)(void *write_user, const uint8_t *data, size_t size);

/* 执行固件流式解压:将 firmware_path (.lzma 包) 解压到 output_path
 * 成功返回 0,失败返回负数错误码(实现在 unzip_stream.c)
 * 仅 HOST_SIM 可用(依赖 fopen/fwrite) */
#ifdef HOST_SIM
int perform_firmware_update(const char *firmware_path, const char *output_path);

/* 真流式解压:解压数据不落盘,逐块写入 write_fn(如直接写 Flash)
 * 内部完成固件头部校验与 CRC32 校验,成功返回 0,失败返回负数错误码
 * out_crc32/out_total 可为 NULL
 * 仅 HOST_SIM 可用 */
int perform_firmware_update_stream(const char *firmware_path,
                                   fw_output_write_fn write_fn, void *write_user,
                                   uint32_t *out_crc32, uint64_t *out_total);
#endif

/* 从 firmware_source_t(MCU SPI Flash 或 PC 文件后端)执行流式解压
 * - HOST_SIM 与 MCU 通用入口
 * - 内部读头 + 校验 + 流式 LZMA 解压 + CRC32 校验
 * - write_fn 回调用于把解压数据逐块写往任意目标(Flash/文件/null)
 * - 成功返回 0,失败返回负数错误码
 * - out_crc32/out_total 可为 NULL */
int perform_firmware_update_from_source(firmware_source_t *src,
                                        fw_output_write_fn write_fn,
                                        void *write_user,
                                        uint32_t *out_crc32,
                                        uint64_t *out_total);

#endif