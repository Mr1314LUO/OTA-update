/*
 * firmware_create.c - 创建固件更新包工具
 *
 * 功能：将原始固件二进制文件打包为 LZMA 压缩的固件更新包
 * 格式：[FirmwareHeader][5-byte LZMA props][raw LZMA1 encoded stream]
 *
 * 编码器实现：使用系统 <lzma.h> (liblzma / XZ Utils) 的 LZMA1 过滤器，
 *             与 lzma/unzip/unzip.c 保持一致，依赖 -llzma 链接。
 *             输出格式与 lzma/flow-unzip/unzip_stream.c 的 bundled SDK
 *             LzmaDec.c 解码器完全兼容（5-byte props + 原始流）。
 *
 * 独立 CLI 编译：
 *   gcc -DENABLE_ZIP_CLI zip.c ../sdk/7zCrc.c ../sdk/7zCrcOpt.c \
 *       ../sdk/Alloc.c -I../sdk -I. -o firmware_create -llzma
 *
 * 使用：./firmware_create <input_firmware.bin> <output_firmware.lzma> [version]
 */
#include "zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lzma.h>

/* 编码输出缓冲区大小（流式写盘） */
#define ZIP_ENCODE_OUTBUF_SIZE  (64 * 1024)

/* LZMA 编码参数（与 7-Zip SDK 原默认值对齐，解码器从 props 读取） */
#define ZIP_LZMA_DICT_SIZE      (1u << 20)   /* 1 MB 字典 */
#define ZIP_LZMA_LC             3            /* 字面量上下文比特数 */
#define ZIP_LZMA_LP             0            /* 字面量位置比特数 */
#define ZIP_LZMA_PB             2            /* 位置比特数 */
#define ZIP_LZMA_PRESET_LEVEL   5            /* 压缩级别 0~9 */

/* 将 lzma_ret 转换为可读字符串（用于错误信息） */
static const char *lzma_strerror_short(lzma_ret r)
{
    switch (r) {
    case LZMA_OK:                return "OK";
    case LZMA_STREAM_END:        return "STREAM_END";
    case LZMA_MEM_ERROR:         return "内存分配失败";
    case LZMA_MEMLIMIT_ERROR:    return "内存限制不足";
    case LZMA_FORMAT_ERROR:      return "格式错误";
    case LZMA_OPTIONS_ERROR:     return "参数错误";
    case LZMA_DATA_ERROR:        return "数据错误";
    case LZMA_BUF_ERROR:         return "缓冲区错误";
    case LZMA_UNSUPPORTED_CHECK: return "不支持的校验类型";
    case LZMA_PROG_ERROR:        return "程序逻辑错误";
    default:                     return "未知错误";
    }
}

/* 创建固件更新包：
 *   input_path   — 原始固件二进制文件
 *   output_path  — 输出 .lzma 固件包
 *   version      — 写入固件头的版本号
 *
 * 返回 0 成功，负数表示失败（错误码含义与原实现一致） */
int create_firmware_package(
    const char *input_path,
    const char *output_path,
    uint32_t version)
{
    FILE    *input_file   = NULL;
    FILE    *output_file  = NULL;
    uint8_t *input_data   = NULL;
    FirmwareHeader_t header;
    long     file_size;
    uint32_t crc32;
    int      ret_code     = 0;

    /* ---- 1. 打开 & 读取输入文件 ---- */
    printf("=== 固件打包工具 ===\n\n");

    input_file = fopen(input_path, "rb");
    if (!input_file) {
        fprintf(stderr, "错误：无法打开输入文件 '%s'\n", input_path);
        return -1;
    }
    fseek(input_file, 0, SEEK_END);
    file_size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);
    if (file_size <= 0) {
        fprintf(stderr, "错误：输入文件为空\n");
        fclose(input_file);
        return -2;
    }
    printf("输入文件: %s\n", input_path);
    printf("文件大小: %ld bytes\n", file_size);

    input_data = (uint8_t *)malloc((size_t)file_size);
    if (!input_data) {
        fprintf(stderr, "错误：内存分配失败\n");
        fclose(input_file);
        return -3;
    }
    if (fread(input_data, 1, (size_t)file_size, input_file) != (size_t)file_size) {
        fprintf(stderr, "错误：读取文件失败\n");
        free(input_data);
        fclose(input_file);
        return -4;
    }
    fclose(input_file);
    input_file = NULL;

    /* ---- 2. 计算未压缩数据的 CRC32（使用 7z SDK 的 CRC 实现） ---- */
    CrcGenerateTable();
    crc32 = CrcCalc(input_data, (size_t)file_size);
    printf("CRC32: 0x%08X\n", crc32);

    /* ---- 3. 初始化 LZMA1 编码器 ---- */
    lzma_options_lzma lzma_opts;
    memset(&lzma_opts, 0, sizeof(lzma_opts));
    if (lzma_lzma_preset(&lzma_opts, ZIP_LZMA_PRESET_LEVEL) != LZMA_OK) {
        fprintf(stderr, "错误：LZMA preset 初始化失败\n");
        free(input_data);
        return -8;
    }
    /* 覆盖为与原实现匹配的字典大小；lc/lp/pb 默认已与 7-Zip 对齐 */
    lzma_opts.dict_size = ZIP_LZMA_DICT_SIZE;
    lzma_opts.lc        = ZIP_LZMA_LC;
    lzma_opts.lp        = ZIP_LZMA_LP;
    lzma_opts.pb        = ZIP_LZMA_PB;

    lzma_filter filters[2];
    filters[0].id      = LZMA_FILTER_LZMA1;
    filters[0].options = &lzma_opts;
    filters[1].id      = LZMA_VLI_UNKNOWN;
    filters[1].options = NULL;

    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_ret lret = lzma_raw_encoder(&strm, filters);
    if (lret != LZMA_OK) {
        fprintf(stderr, "错误：LZMA 编码器初始化失败 (%s)\n", lzma_strerror_short(lret));
        free(input_data);
        return -8;
    }

    /* ---- 4. 构造 5 字节 LZMA 属性（与 SDK 的 LZMA_PROPS_SIZE=5 一致）
     *   byte 0   : (pb * 5 + lp) * 9 + lc
     *   bytes 1-4: dictSize （小端序 uint32） ---- */
    uint8_t lzma_props[5];
    lzma_props[0] = (uint8_t)((ZIP_LZMA_PB * 5 + ZIP_LZMA_LP) * 9 + ZIP_LZMA_LC);
    uint32_t dict_size_le = lzma_opts.dict_size;
    lzma_props[1] = (uint8_t)(dict_size_le & 0xFF);
    lzma_props[2] = (uint8_t)((dict_size_le >> 8)  & 0xFF);
    lzma_props[3] = (uint8_t)((dict_size_le >> 16) & 0xFF);
    lzma_props[4] = (uint8_t)((dict_size_le >> 24) & 0xFF);

    /* ---- 5. 打开输出文件并写入占位头部 + 属性 ---- */
    output_file = fopen(output_path, "wb");
    if (!output_file) {
        fprintf(stderr, "错误：无法创建输出文件 '%s'\n", output_path);
        free(input_data);
        lzma_end(&strm);
        return -5;
    }

    memset(&header, 0, sizeof(header));
    header.magic             = FIRMWARE_MAGIC;
    header.version           = version;
    header.uncompressed_size = (uint32_t)file_size;
    header.crc32             = crc32;
    /* compressed_size 稍后回填 */

    if (fwrite(&header, 1, sizeof(header), output_file) != sizeof(header)) {
        fprintf(stderr, "错误：写入头部失败\n");
        ret_code = -6; goto cleanup;
    }
    if (fwrite(lzma_props, 1, sizeof(lzma_props), output_file) != sizeof(lzma_props)) {
        fprintf(stderr, "错误：写入 LZMA 属性失败\n");
        ret_code = -11; goto cleanup;
    }

    /* ---- 6. 运行 LZMA1 流式编码器 ---- */
    printf("开始压缩...\n");

    uint8_t *out_buf = (uint8_t *)malloc(ZIP_ENCODE_OUTBUF_SIZE);
    if (!out_buf) {
        fprintf(stderr, "错误：编码缓冲区分配失败\n");
        ret_code = -3; goto cleanup;
    }

    strm.next_in   = input_data;
    strm.avail_in  = (size_t)file_size;
    strm.next_out  = out_buf;
    strm.avail_out = ZIP_ENCODE_OUTBUF_SIZE;

    lzma_action action = LZMA_RUN;

    for (;;) {
        /* 输入耗尽后，切换为 FINISH 以刷新编码器尾部 */
        if (strm.avail_in == 0)
            action = LZMA_FINISH;

        lret = lzma_code(&strm, action);

        /* 输出缓冲区已满或流结束：写盘并重置 */
        if (strm.avail_out == 0 || lret == LZMA_STREAM_END) {
            size_t to_write = ZIP_ENCODE_OUTBUF_SIZE - strm.avail_out;
            if (to_write > 0) {
                if (fwrite(out_buf, 1, to_write, output_file) != to_write) {
                    fprintf(stderr, "错误：写入压缩数据失败\n");
                    ret_code = -7;
                    free(out_buf);
                    goto cleanup;
                }
            }
            strm.next_out  = out_buf;
            strm.avail_out = ZIP_ENCODE_OUTBUF_SIZE;
        }

        if (lret == LZMA_STREAM_END)
            break;
        if (lret != LZMA_OK) {
            fprintf(stderr, "错误：压缩失败 (%s)\n", lzma_strerror_short(lret));
            ret_code = -12;
            free(out_buf);
            goto cleanup;
        }
    }

    free(out_buf);
    out_buf = NULL;

    /* ---- 7. 回填 compressed_size（头部之后所有字节，含 5 字节属性） ---- */
    long total_after_header = ftell(output_file) - (long)sizeof(header);
    if (total_after_header < 0)
        total_after_header = 0;

    header.compressed_size = (uint32_t)total_after_header;

    if (fseek(output_file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "错误：回写头部寻址失败\n");
        ret_code = -6; goto cleanup;
    }
    if (fwrite(&header, 1, sizeof(header), output_file) != sizeof(header)) {
        fprintf(stderr, "错误：回写头部失败\n");
        ret_code = -6; goto cleanup;
    }

    printf("压缩完成: %ld bytes -> %lu bytes\n",
           file_size, (unsigned long)total_after_header);
    printf("压缩率: %.2f%%\n",
           (file_size > 0)
               ? (1.0 - (double)total_after_header / (double)file_size) * 100.0
               : 0.0);
    printf("输出文件: %s\n", output_path);

    ret_code = 0;

cleanup:
    lzma_end(&strm);
    if (output_file) fclose(output_file);
    free(input_data);
    /* 失败时清理不完整的输出文件 */
    if (ret_code != 0)
        remove(output_path);
    return ret_code;
}

/* 压缩固件包（供 OTA FSM 调用）
 * 将 input_path 原始文件压缩为 output_path (.lzma 包)
 * version_str 为版本字符串，使用 atoi() 解析（如 "1"、"100"、"1.0"→1）
 * 成功返回 0，失败返回负数（与 create_firmware_package 错误码一致） */
int compressed_File(const char *input_path, const char *output_path,
                    const char *version_str)
{
    uint32_t version = 1;
    if (version_str && version_str[0] != '\0') {
        version = (uint32_t)atoi(version_str);
    }
    return create_firmware_package(input_path, output_path, version);
}

/* ==================== 独立 CLI 入口 ====================
 * 仅在定义 ENABLE_ZIP_CLI 时编译（例如在主机上单独打包固件）：
 *   gcc -DENABLE_ZIP_CLI zip.c <sdk objs> -I<sdk> -o firmware_create -llzma
 * 集成到 OTA 主程序时不定义该宏，避免与 app/ota_main.c 的 main() 冲突。 */
#ifdef ENABLE_ZIP_CLI
static void print_usage(const char *program_name)
{
    printf("用法: %s <输入文件.bin> <输出文件.lzma> [版本号]\n", program_name);
    printf("\n示例:\n");
    printf("  %s firmware.bin firmware_v1.0.lzma 100\n", program_name);
    printf("\n说明:\n");
    printf("  输入文件为原始固件二进制文件\n");
    printf("  输出文件为 LZMA 压缩的固件更新包\n");
    printf("  版本号为可选参数，默认为 1\n");
}

int main(int argc, char *argv[])
{
    if (argc < 3 || argc > 4) {
        print_usage(argv[0]);
        return 1;
    }
    const char *input_path  = argv[1];
    const char *output_path = argv[2];
    uint32_t    version     = 1;
    if (argc == 4)
        version = (uint32_t)atoi(argv[3]);

    int ret = create_firmware_package(input_path, output_path, version);
    if (ret != 0) {
        fprintf(stderr, "\n固件打包失败 (错误代码: %d)\n", ret);
        return 1;
    }
    return 0;
}
#endif /* ENABLE_ZIP_CLI */
