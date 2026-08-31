#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include "firmware_update.h"

// 前置声明
static int flashing_firmware(const firmware_partition_entry_t *entry, const char *file_path, FILE *fp, uint8_t *buf, long file_size);

// 固件分区表 - 可按需增删修改
static const firmware_partition_entry_t firmware_partition_table[] = {
    {"boot",      "boot.bin",      0x08000000, 64 * 1024,   1},
    {"boot",      "boot.bin.lzma",      0x08000000, 64 * 1024,   1},
    {"app",       "app.bin",       0x08010000, 256 * 1024,  2},
    {"hal",       "hal.bin",       0x08050000, 128 * 1024,  3},
    {"wifi_fw",   "wifi_fw.bin",   0x08070000, 64 * 1024,   4},
    {"fs",        "fs.bin",        0x08080000, 128 * 1024,  5},
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

// 从文件读取固件数据并写入Flash分区
static int flash_firmware_from_file(const firmware_partition_entry_t *entry, const char *file_path)
{
    uint8_t *buf = (uint8_t *)malloc(FIRMWARE_UPDATE_BUF_SIZE);
    // 打开文件
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        printf("[ERROR] Failed to open firmware file: %s\n", file_path);
        return -1;
    }
    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    // 检查文件大小是否超过最大允许大小
    if (file_size <= 0) {
        printf("[ERROR] Firmware file is empty: %s\n", file_path);
        fclose(fp);
        return -1;
    } else
    if ((uint32_t)file_size > entry->max_size) {
        printf("[ERROR] Firmware file size (%ld) exceeds max size (%u) for module: %s\n",
               file_size, entry->max_size, entry->module_name);
        fclose(fp);
        return -1;
    }
    // 打印升级信息
    printf("[INFO] Flashing %s (%ld bytes) to 0x%08X...\n",
           entry->module_name, file_size, entry->flash_start_addr);
    // 擦除Flash分区
    if (!hal_ota_instance.flash_erase(entry->flash_start_addr, entry->max_size)) {
        printf("[ERROR] Failed to erase flash for module: %s\n", entry->module_name);
        fclose(fp);
        return -1;
    } else
    // 分块读取并写入Flash
    if (buf == NULL) {      // 分块读取并写入Flash
        printf("[ERROR] Failed to allocate memory for firmware buffer\n");
        fclose(fp);
        return -1;
    }
    // 调用分块读取并写入Flash函数
    int ret = flashing_firmware(entry, file_path, fp, buf, file_size);
    if (ret == 0) {
        printf("[INFO] Successfully flashed %s (%ld bytes)\n", entry->module_name, file_size);
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
            printf("[ERROR] Failed to read firmware file: %s\n", file_path);
            ret = -1;
            break;
        }
        // 写入Flash
        if (!hal_ota_instance.flash_write(current_addr, buf, bytes_read)) {
            printf("[ERROR] Failed to write flash for module: %s at 0x%08X\n",
                   entry->module_name, current_addr);
            ret = -1;
            break;
        }
        // 更新当前地址和已写入字节数
        current_addr += bytes_read;
        total_written += bytes_read;
        // 打印进度
        int progress = (total_written * 100) / (uint32_t)file_size;
        printf("\r[INFO] Progress: %d%%", progress);
        fflush(stdout);
    }
    // 打印升级完成
    printf("\n");
    printf("[INFO] Successfully flashed %s (%u bytes)\n", entry->module_name, total_written);
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
        printf("[ERROR] Update-Firmware directory not found: %s\n", update_dir);
        return -1;
    }
    // 打印升级信息
    printf("[INFO] Starting firmware update from: %s\n", update_dir);
    printf("[INFO] Found %zu modules in partition table\n\n", FIRMWARE_PARTITION_TABLE_SIZE);
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
            printf("[WARN] Firmware file not found, skipping %s: %s\n",
                   entry->module_name, entry->firmware_file);
            continue;
        }
        printf("[INFO] [%d/%zu] Processing module: %s\n",
               (int)i + 1, FIRMWARE_PARTITION_TABLE_SIZE, entry->module_name);
        // 执行升级
        if (flash_firmware_from_file(entry, file_path) == 0) {
            success_count++;
        } else {
            fail_count++;
            printf("[ERROR] Failed to update module: %s\n", entry->module_name);
            // 升级失败时可选择继续或终止，这里选择继续
            // break;  // 如需失败即终止，取消此行注释
        }
        printf("\n");
    }
    // 输出升级结果汇总
    printf("========================================\n");
    printf("[INFO] Firmware Update Summary:\n");
    printf("[INFO]   Success: %d\n", success_count);
    printf("[INFO]   Failed:  %d\n", fail_count);
    printf("[INFO]   Skipped: %zu\n", FIRMWARE_PARTITION_TABLE_SIZE - success_count - fail_count);
    printf("========================================\n");
    // 如果有升级失败，返回失败
    if (fail_count > 0) {return -1;}
    return 0; // 成功
}