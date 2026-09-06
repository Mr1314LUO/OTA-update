#include "firmware_update.h"
#include "printf.h"

// 固件分区表 - 可按需增删修改
static const firmware_partition_entry_t firmware_partition_table[] = {
    {"boot",           "boot.bin",               0x08000000, 20 * KB ,     1},
    {"hal",            "hal.bin",                0x08050000, 128 * KB,     2},
    {"app",            "app.bin",                0x08010000, 256 * KB,     3},
    {"module",         "module.bin",             0x08020000, 1024 * KB,    4},
    {"nvs",             "nvs.bin",               0x08080000, 64 * KB,      5},
    {"firmware",        "firmware.bin.lzma",     0x08000000, 8 * MB,      6},
    // {"bsp",            "bsp.bin",                0x08070000, 64 * KB,      3},
    // {"user_data",       "user_data.bin",         0x080A0000, 64 * KB,      5},
    // {"config",          "config.bin",            0x080B0000, 32 * KB,      6},
    // {"kernel",          "kernel.bin",            0x08000000, 3 * MB,      8},
    // {"rootfs",         "rootfs.bin",             0x08000000, 5 * MB,      8},
    // {"fs",             "fs.bin",                 0x08080000, 128 * KB,     5},
};

// 检查目录是否存在
static int dir_exists(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 1;
    }
    return 0;
}

// 擦除进度回调：实时打印擦除百分比（使用 \r 覆盖同一行）
static void erase_progress_print(uint32_t progress, const char *module_name)
{
    LOG_INFO("\r 🚀🚀🚀 擦除进度 Erasing %s... %u%% ", module_name, progress);

    fflush(stdout);
}

// 解压输出回调：将解压数据逐块直接写入 Flash
static int flash_output_write(void *write_user, const uint8_t *data, size_t size)
{
    flash_write_ctx_t *ctx = (flash_write_ctx_t *)write_user;
    uint32_t current_addr = ctx->start_addr + ctx->written;

    if (ctx->written + size > ctx->max_size) {
        LOG_ERROR("Decompressed firmware exceeds max size (%u) for module: %s\n",
                  ctx->max_size, ctx->module_name);
        return -1;
    }
    // 调用 HAL 层写入 Flash
    if (!hal_ota_instance.flash_write(current_addr, data, (uint32_t)size)) {
        LOG_ERROR("Failed to write flash for module: %s at 0x%08X\n",
                  ctx->module_name, current_addr);
        return -1;
    }
    ctx->written += (uint32_t)size;
    return 0;
}

// 从升级包文件写入Flash分区：
// .lzma 压缩包 -> 擦除后流式解压直接写 Flash（不落盘）；普通 .bin -> 分块读取写入
static int flash_firmware_from_file(const firmware_partition_entry_t *entry, const char *file_path)
{
    uint32_t magic = 0;
    int ret = 0;

    // 读取文件头 4 字节判断是否为 .lzma 固件包（魔数 "FRWF"）
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        LOG_ERROR("Failed to open firmware file: %s\n", file_path);
        return -1;
    }
    if (fread(&magic, 1, sizeof(magic), fp) != sizeof(magic)) {
        LOG_ERROR("Failed to read firmware file header: %s\n", file_path);
        fclose(fp);
        return -1;
    }
    fclose(fp);

    if (magic == FIRMWARE_MAGIC) {
        /* ---- LZMA 压缩包：真流式升级，解压数据直接写 Flash ---- */
        LOG_INFO("Flashing %s (lzma stream) to 0x%08X...\n",
                 entry->module_name, entry->flash_start_addr);
        // 擦除Flash分区（按页擦除并实时打印百分比进度）
        if (!hal_ota_instance.flash_erase(entry->flash_start_addr, entry->max_size,
                                          erase_progress_print, entry->module_name)) {
            printf("\n");
            LOG_ERROR("Failed to erase flash for module: %s\n", entry->module_name);
            return -1;
        }
        LOG_INFO("\r 🚀🚀🚀 擦除进度 Erase %s complete: 100%%\n ", entry->module_name);
        // 流式解压并逐块写入Flash（内部完成固件头部与CRC32校验）
        flash_write_ctx_t ctx = {
            entry->flash_start_addr, entry->max_size, 0, entry->module_name
        };
        uint32_t calc_crc  = 0;
        uint64_t total_out = 0;
        ret = perform_firmware_update_stream(file_path, flash_output_write, &ctx,
                                             &calc_crc, &total_out);
        if (ret == 0) {
            LOG_SUCCESS("\n🟢 Successfully flashed %s (%lu bytes) to 0x%08X\n",
                     entry->module_name, (unsigned long)total_out, entry->flash_start_addr);
        }
        return ret;
    }

    /* ---- 普通bin：保持原有分块读取写入流程 ---- */
    // 打开文件
    fp = fopen(file_path, "rb");
    if (fp == NULL) {
        LOG_ERROR("Failed to open firmware file: %s\n", file_path);
        return -1;
    }
    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    // 检查文件大小是否超过最大允许大小
    if (file_size <= 0) {
        LOG_ERROR("Firmware file is empty: %s\n", file_path);
        fclose(fp);
        return -1;
    } else
    if ((uint32_t)file_size > entry->max_size) {
        LOG_ERROR("Firmware file size (%ld) exceeds max size (%u) for module: %s\n",
                  file_size, entry->max_size, entry->module_name);
        fclose(fp);
        return -1;
    }
    // 分配分块写入缓冲区（在擦除Flash之前分配，避免失败路径遗漏释放）
    uint8_t *buf = (uint8_t *)malloc(FIRMWARE_UPDATE_BUF_SIZE);
    if (buf == NULL) {
        LOG_ERROR("Failed to allocate memory for firmware buffer\n");
        fclose(fp);
        return -1;
    }
    // 打印升级信息
    LOG_INFO("Flashing %s (%ld bytes) to 0x%08X...\n",
             entry->module_name, file_size, entry->flash_start_addr);
    // 擦除Flash分区（按页擦除并实时打印百分比进度）
    if (!hal_ota_instance.flash_erase(entry->flash_start_addr, entry->max_size,
                                      erase_progress_print, entry->module_name)) {
        printf("\n");
        LOG_ERROR("Failed to erase flash for module: %s\n", entry->module_name);
        fclose(fp);
        free(buf);
        return -1;
    }
    LOG_INFO("\r 🚀🚀🚀 擦除进度 Erase %s complete: 100%%\n", entry->module_name);
    // 分块读取并写入Flash
    ret = flashing_firmware(entry, file_path, fp, buf, file_size);
    if (ret == 0) {
        LOG_INFO("Successfully flashed %s (%ld bytes)\n", entry->module_name, file_size);
    }

    // 关闭文件
    fclose(fp);
    return ret;
}


static int flashing_firmware(const firmware_partition_entry_t *entry, const char *file_path, FILE *fp, uint8_t *buf, long file_size)
{
    uint32_t total_written = 0;
    uint32_t current_addr = entry->flash_start_addr;
    int ret = 0;

    // 分块读取并写入Flash
    while (total_written < (uint32_t)file_size)
    {
        uint32_t to_read = FIRMWARE_UPDATE_BUF_SIZE;
        if (total_written + to_read > (uint32_t)file_size) {
            to_read = (uint32_t)file_size - total_written;
        }
        // 读取固件数据到缓冲区
        size_t bytes_read = fread(buf, 1, to_read, fp);
        if (bytes_read != to_read) {
            LOG_ERROR("Failed to read firmware file: %s\n", file_path);
            ret = -1;
            break;
        }
        // 写入Flash
        if (!hal_ota_instance.flash_write(current_addr, buf, bytes_read)) {
            LOG_ERROR("Failed to write flash for module: %s at 0x%08X\n",
                      entry->module_name, current_addr);
            ret = -1;
            break;
        }
        // 更新当前地址和已写入字节数
        current_addr += bytes_read;
        total_written += bytes_read;
        // 打印进度
        int progress = (total_written * 100) / (uint32_t)file_size;
        LOG_INFO("\r 🚀🚀🚀 写入进度 Write Progress: %d%%", progress);
        fflush(stdout);
    }
    // 打印升级结果（仅在成功时输出成功信息）
    printf("\n");
    if (ret == 0) {
        LOG_INFO("Successfully flashed %s (%u bytes)\n", entry->module_name, total_written);
    }
    // 释放缓冲区内存
    free(buf);
    return ret;
}

// 表格法固件升级主函数
int firmware_update(void)
{
    const char *update_dir = UPDATE_F_DIR;
    int success_count = 0;
    int fail_count = 0;

    // 检查升级目录是否存在
    if (!dir_exists(update_dir)) {
        LOG_ERROR("Firmware directory not found: %s\n", update_dir);
        return -1;
    }
    // 打印升级信息
    LOG_INFO("Starting firmware update from: %s\n", update_dir);
    LOG_INFO("Found %zu modules in partition table\n", FIRMWARE_PARTITION_TABLE_SIZE);
    // 遍历分区表，按优先级顺序升级
    for (uint32_t i = 0; i < FIRMWARE_PARTITION_TABLE_SIZE; i++) {
        // 获取当前模块的分区表条目
        const firmware_partition_entry_t *entry = &firmware_partition_table[i];
        // 构建固件文件完整路径
        char file_path[512];
        snprintf(file_path, sizeof(file_path), "%s/%s", update_dir, entry->firmware_file);
        // 检查固件文件是否存在
        struct stat st;
        if (stat(file_path, &st) != 0) {
            LOG_WARN("Firmware file not found, skipping %s: %s\n",
                     entry->module_name, entry->firmware_file);
            continue;
        }
        LOG_INFO("[%d/%zu] Processing module: %s\n",
                 (int)i + 1, FIRMWARE_PARTITION_TABLE_SIZE, entry->module_name);
        // 执行升级
        if (flash_firmware_from_file(entry, file_path) == 0) {
            success_count++;
        } else {
            fail_count++;
            LOG_ERROR("Failed to update module: %s\n", entry->module_name);
            // 升级失败时可选择继续或终止，这里选择继续
            // break;  // 如需失败即终止，取消此行注释
        }
        printf("\n");
    }
    // 输出升级结果汇总
    printf("===============升级结果=================\n");
    LOG_INFO("Firmware Update Summary:\n");
    LOG_INFO("  Success: %d\n", success_count);
    LOG_INFO("  Failed:  %d\n", fail_count);
    LOG_INFO("  Skipped: %zu\n", FIRMWARE_PARTITION_TABLE_SIZE - success_count - fail_count);
    printf("========================================\n");
    // 如果有升级失败，返回失败
    if (fail_count > 0) {return -1;}
    return 0; // 成功
}