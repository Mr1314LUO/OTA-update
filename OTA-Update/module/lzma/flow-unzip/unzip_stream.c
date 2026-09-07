/*
 * flow-unzip.c - 固件流式解压工具(HOST_SIM CLI + MCU Bootloader 通用)
 *
 * 本程序直接从 LZMA 固件包解压出原始固件:
 * 1. 读取固件头(从 firmware_source_t:PC 文件 / MCU SPI Flash)
 * 2. 验证固件头部(魔数、版本、CRC32)
 * 3. 使用 LZMA 流式解码器解压固件数据(INPUT/OUTPUT_BUFFER_SIZE 缓冲)
 * 4. 将解压后的固件逐块写入 write_fn 回调(文件/Flash/任意目标)
 *
 * 编译:
 *   HOST_SIM CLI: gcc -DFLOW_UNZIP_STANDALONE -DHOST_SIM ...
 *   MCU Bootloader: arm-none-eabi-gcc -DINPUT_BUFFER_SIZE=512 ...
 *
 * 使用:./flow-unzip <输入固件.lzma> [输出固件.bin]
 */

#include "unzip_stream.h"
#include "printf.h"  /* !HOST_SIM 时重定向 printf/snprintf 到 printf_lite */

/* ================================================================
 * 文件输出回调(HOST_SIM only)
 * ================================================================ */
#ifdef HOST_SIM
static int file_write_cb(void *write_user, const uint8_t *data, size_t size)
{
    FILE *fp = (FILE *)write_user;
    if (fwrite(data, 1, size, fp) != size) {
        fprintf(stderr, "错误：写入输出文件失败\n");
        return -1;
    }
    return 0;
}
#endif /* HOST_SIM */

/* ================================================================
 * 进度回调函数
 * ================================================================ */
static int my_progress_callback(uint64_t in, uint64_t out, void *user_data)
{
    (void)in; /* 输入字节数暂不用于进度计算 */
    uint64_t total_uncompressed = *(uint64_t *)user_data;
    int percent = 0;

    if (total_uncompressed > 0) {
        percent = (int)(out * 100 / total_uncompressed);
    }

    /* 仅在百分比变化时更新显示,避免刷屏 */
    if (percent != g_last_percent) {
        printf("\r  解压进度: %3d%%  (%lu / %lu bytes)",
               percent, (unsigned long)out, (unsigned long)total_uncompressed);
#ifdef HOST_SIM
        fflush(stdout);
#endif
        g_last_percent = percent;
    }

    return 0; /* 返回 0 表示继续,返回非 0 可取消操作 */
}

/* ================================================================
 * 流式解压:LZMA -> write_fn 回调(文件/Flash 等任意输出),
 * 同时增量计算 CRC32
 *
 * 输入源:firmware_source_t(MCU SPI Flash 或 PC 文件后端)
 * ================================================================ */
static int decompress_firmware_stream(
    firmware_source_t *src,
    fw_output_write_fn write_fn, void *write_user,
    const FirmwareHeader_t *header,
    uint32_t *out_crc32, uint64_t *out_total)
{
    CLzmaDec      lzma_state;
    SRes          lzma_res = SZ_OK;
    /* static:避免在 MCU 上占用栈(INPUT/OUTPUT_BUFFER_SIZE=512 时已小,
     * 但仍用 static 避免与其他栈帧叠加) */
    static uint8_t in_buf[INPUT_BUFFER_SIZE];
    static uint8_t out_buf[OUTPUT_BUFFER_SIZE];
    uint8_t       lzma_props[LZMA_PROPS_SIZE];
    size_t        in_pos = 0;
    size_t        in_size = 0;
    size_t        out_pos = 0;
    uint64_t      remaining = header->uncompressed_size;
    uint64_t      total_in = 0;
    uint64_t      total_out = 0;
    UInt32        crc = CRC_INIT_VAL;
    uint32_t      src_offset = sizeof(FirmwareHeader_t);  /* 跳过固件头 */

    CrcGenerateTable();

    /* 读 LZMA props(5 字节) */
    int n = src->read(src->ctx, src_offset, lzma_props, LZMA_PROPS_SIZE);
    if (n != LZMA_PROPS_SIZE) {
        printf("错误:无法读取 LZMA 压缩属性\n");
        return -8;
    }
    src_offset += LZMA_PROPS_SIZE;

    LzmaDec_Construct(&lzma_state);
    lzma_res = LzmaDec_Allocate(&lzma_state, lzma_props, LZMA_PROPS_SIZE,
                                &g_FirmwareAlloc);
    if (lzma_res != SZ_OK) {
        printf("错误:LZMA 解码器初始化失败 (代码: %d)\n", lzma_res);
        return -9;
    }

    LzmaDec_Init(&lzma_state);

    while (remaining > 0) {
        if (in_pos == in_size) {
            /* 从源读一块输入 */
            n = src->read(src->ctx, src_offset, in_buf, sizeof(in_buf));
            if (n <= 0) {
                printf("\n错误:输入数据不完整\n");
                lzma_res = SZ_ERROR_DATA;
                break;
            }
            in_size = (size_t)n;
            src_offset += in_size;
            in_pos = 0;
        }

        SizeT src_processed = in_size - in_pos;
        SizeT dst_processed = OUTPUT_BUFFER_SIZE - out_pos;
        ELzmaFinishMode finish = LZMA_FINISH_ANY;
        ELzmaStatus status;

        if (dst_processed > remaining) {
            dst_processed = (SizeT)remaining;
            finish = LZMA_FINISH_END;
        }

        lzma_res = LzmaDec_DecodeToBuf(
            &lzma_state,
            out_buf + out_pos, &dst_processed,
            in_buf + in_pos, &src_processed,
            finish, &status
        );

        in_pos += src_processed;
        out_pos += dst_processed;
        remaining -= dst_processed;
        total_in += src_processed;
        total_out += dst_processed;

        if (out_pos > 0) {
            if (write_fn(write_user, out_buf, out_pos) != 0) {
                lzma_res = SZ_ERROR_WRITE;
                break;
            }
            crc = CrcUpdate(crc, out_buf, out_pos);
            out_pos = 0;
        }

        {
            uint64_t total = header->uncompressed_size;
            my_progress_callback(total_in, total_out, &total);
        }

        if (lzma_res != SZ_OK) {
            if (lzma_res == SZ_ERROR_DATA) {
                printf("\n错误:LZMA 数据损坏\n");
            }
            break;
        }

        if (src_processed == 0 && dst_processed == 0) {
            printf("\n错误:解码停滞\n");
            lzma_res = SZ_ERROR_DATA;
            break;
        }
    }

    printf("\n\n");
    LzmaDec_Free(&lzma_state, &g_FirmwareAlloc);

    if (lzma_res != SZ_OK) {
        printf("错误:解压失败 (代码: %d)\n", lzma_res);
        return -10;
    }

    if (total_out != header->uncompressed_size) {
        printf("错误:解压大小不匹配 (期望 %u, 实际 %lu)\n",
                header->uncompressed_size, (unsigned long)total_out);
        return -11;
    }

    g_total_uncompressed = total_out;

    if (out_crc32) {
        *out_crc32 = CRC_GET_DIGEST(crc);
    }
    if (out_total) {
        *out_total = total_out;
    }

    return 0;
}

/* ================================================================
 * 从 firmware_source_t 执行流式解压(HOST_SIM + MCU 通用入口)
 *
 * 输入:src       - 固件源(SPI Flash / 文件后端)
 *       write_fn  - 解压输出回调(返回非 0 中止)
 *       write_user - 回调用户上下文
 * 功能:
 *   1. 从源读取并验证固件头部
 *   2. 解压 LZMA 数据逐块写入 write_fn(不落盘)
 *   3. 验证 CRC32
 * ================================================================ */
int perform_firmware_update_from_source(firmware_source_t *src,
                                        fw_output_write_fn write_fn,
                                        void *write_user,
                                        uint32_t *out_crc32,
                                        uint64_t *out_total)
{
    FirmwareHeader_t header;
    uint32_t        calc_crc  = 0;
    uint64_t        total_out = 0;
    int             ret       = 0;

    printf("========================== 固件流式解压工具 =====================\n");

    /* ---- 步骤 1: 读取固件头部 ---- */
    printf("[1/4] 读取固件头部\n");

    int n = src->read(src->ctx, 0, (uint8_t *)&header, sizeof(FirmwareHeader_t));
    if (n != (int)sizeof(FirmwareHeader_t)) {
        printf("错误:无法读取固件头部 (读到 %d 字节)\n", n);
        return -3;
    }

    /* ---- 步骤 2: 验证固件头部 ---- */
    printf("\n[2/4] 验证固件头部...\n");

    if (header.magic != FIRMWARE_MAGIC) {
        printf("错误:无效的固件魔数 (期望: 0x%08X, 实际: 0x%08X)\n",
                FIRMWARE_MAGIC, header.magic);
        return -4;
    }

    if (header.compressed_size == 0 || header.uncompressed_size == 0) {
        printf("错误:固件大小无效 (压缩: %u, 未压缩: %u)\n",
                header.compressed_size, header.uncompressed_size);
        return -5;
    }

    g_total_uncompressed = header.uncompressed_size;

    {
        char ver_buf[FW_VERSION_STR_LEN];
        printf("  固件版本: %s\n", fw_version_display(&header, ver_buf, sizeof(ver_buf)));
        printf("  压缩大小: %u bytes\n", header.compressed_size);
        printf("  未压缩大小: %u bytes\n", header.uncompressed_size);
        printf("  CRC32:    0x%08X\n", header.crc32);
    }

    /* ---- 步骤 3: 流式解压,逐块写入 write_fn ---- */
    printf("\n[3/4] 开始流式解压 (输入 %u B, 输出 %u B)...\n",
           INPUT_BUFFER_SIZE, OUTPUT_BUFFER_SIZE);

    ret = decompress_firmware_stream(src, write_fn, write_user,
                                     &header, &calc_crc, &total_out);
    if (ret != 0) {
        return ret;
    }

    /* ---- 步骤 4: 验证 CRC32 ---- */
    printf("[4/4] 验证 CRC32...\n");

    if (calc_crc == header.crc32) {
        printf("  CRC32 验证通过: 0x%08X\n", calc_crc);
    } else {
        printf("  CRC32 验证失败!\n");
        printf("    期望: 0x%08X\n", header.crc32);
        printf("    实际: 0x%08X\n", calc_crc);
        return -12;
    }

    /* ---- 完成 ---- */
    printf("===================== 固件流式解压成功完成!===================\n");
    {
        char ver_buf[FW_VERSION_STR_LEN];
        printf("  固件版本: %s\n", fw_version_display(&header, ver_buf, sizeof(ver_buf)));
        printf("  解压大小: %lu bytes\n", (unsigned long)total_out);
        printf("===============================================================\n");
    }

    if (out_crc32) {
        *out_crc32 = calc_crc;
    }
    if (out_total) {
        *out_total = total_out;
    }

    return 0;
}

/* ================================================================
 * 以下为 HOST_SIM 专用:文件路径入口 + 独立 CLI 工具
 * ================================================================ */
#ifdef HOST_SIM

/* 文件后端 firmware_source 适配器 */
typedef struct {
    FILE *fp;
} file_src_ctx_t;

static uint32_t file_src_size(void *ctx) {
    file_src_ctx_t *c = (file_src_ctx_t *)ctx;
    long cur = ftell(c->fp);
    fseek(c->fp, 0, SEEK_END);
    long sz = ftell(c->fp);
    fseek(c->fp, cur, SEEK_SET);
    return (uint32_t)sz;
}

static int file_src_read(void *ctx, uint32_t offset, uint8_t *buf, uint32_t len) {
    file_src_ctx_t *c = (file_src_ctx_t *)ctx;
    fseek(c->fp, (long)offset, SEEK_SET);
    return (int)fread(buf, 1, len, c->fp);
}

/* 真流式解压:从 firmware_path(.lzma 包)解压,数据不落盘,逐块写入 write_fn */
int perform_firmware_update_stream(const char *firmware_path,
                                   fw_output_write_fn write_fn, void *write_user,
                                   uint32_t *out_crc32, uint64_t *out_total)
{
    FILE *in_fp = fopen(firmware_path, "rb");
    if (!in_fp) {
        fprintf(stderr, "错误：无法打开输入文件 '%s'\n", firmware_path);
        return -2;
    }

    file_src_ctx_t fctx = { .fp = in_fp };
    firmware_source_t src = {
        .ctx  = &fctx,
        .size = file_src_size,
        .read = file_src_read,
    };

    int ret = perform_firmware_update_from_source(&src, write_fn, write_user,
                                                  out_crc32, out_total);
    fclose(in_fp);
    return ret;
}

/* 文件输出封装:解压 .lzma 包到输出文件(独立 CLI 工具使用) */
int perform_firmware_update(const char *firmware_path, const char *output_path)
{
    FILE     *out_fp  = NULL;
    uint32_t  calc_crc = 0;
    uint64_t  total_out = 0;
    int       ret;

    out_fp = fopen(output_path, "wb");
    if (!out_fp) {
        fprintf(stderr, "错误：无法创建输出文件 '%s'\n", output_path);
        return -6;
    }

    ret = perform_firmware_update_stream(firmware_path,
                                         file_write_cb, out_fp,
                                         &calc_crc, &total_out);
    fclose(out_fp);
    if (ret != 0) {
        return ret;
    }

    printf("  输出文件: %s\n", output_path);
    return 0;
}

#endif /* HOST_SIM */

/* ================================================================
 * 独立 CLI 工具外壳(HOST_SIM + FLOW_UNZIP_STANDALONE)
 * 默认作为库链接进 ota_main(不编译 main,避免入口冲突);
 * 独立编译工具时加 -DFLOW_UNZIP_STANDALONE
 * ================================================================ */
#if defined(HOST_SIM) && defined(FLOW_UNZIP_STANDALONE)

/* 生成默认输出文件名:将 .lzma 扩展名替换为 .unpacked */
static void make_output_name(const char *in_path, char *out_buf, size_t buf_size)
{
    const char *dot = strrchr(in_path, '.');
    size_t base_len;

    if (dot && strcmp(dot, ".lzma") == 0) {
        base_len = (size_t)(dot - in_path);
    } else {
        base_len = strlen(in_path);
    }

    if (base_len + 10 > buf_size) {
        base_len = buf_size - 10;
    }

    strncpy(out_buf, in_path, base_len);
    out_buf[base_len] = '\0';
    strcat(out_buf, ".unpacked");
}

/* 打印使用说明 */
static void print_usage(const char *program_name)
{
    printf("用法: %s <输入固件.lzma> [输出固件.bin]\n", program_name);
    printf("\n示例:\n");
    printf("  # 1. 创建测试固件 (100KB 随机数据)\n");
    printf("  dd if=/dev/urandom of=test_firmware.bin bs=1024 count=100\n\n");
    printf("  # 2. 打包固件\n");
    printf("  ./build/firmware_create test_firmware.bin test_firmware.lzma V1.1\n\n");
    printf("  # 3. 运行本程序解压固件\n");
    printf("  %s test_firmware.lzma\n", program_name);
    printf("  或: %s test_firmware.lzma output.bin\n", program_name);
    printf("\n说明:\n");
    printf("  本程序将 LZMA 压缩固件包解压为原始固件文件:\n");
    printf("  - 使用 LZMA 流式解压(低内存占用,4KB 输入/输出缓冲)\n");
    printf("  - 验证 CRC32 校验和\n");
    printf("  - 显示实时解压进度\n");
    printf("  - 如果不指定输出文件名,自动生成 .unpacked 后缀\n");
}

/* 主函数 */
int main(int argc, char *argv[])
{
    if (argc < 2 || argc > 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *input_path = argv[1];
    char default_output[1024];

    const char *output_path = (argc == 3) ? argv[2] : NULL;

    /* 如果用户没有指定输出文件名,自动生成 */
    if (!output_path) {
        make_output_name(input_path, default_output, sizeof(default_output));
        output_path = default_output;
    }

    int ret = perform_firmware_update(input_path, output_path);

    if (ret != 0) {
        fprintf(stderr, "\n固件解压失败 (错误代码: %d)\n", ret);
        return 1;
    }

    return 0;
}

#endif /* HOST_SIM && FLOW_UNZIP_STANDALONE */
