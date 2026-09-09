// main.c —— App 固件入口（模板）
//
// 流程：硬件初始化(UART/外部存储) → 检查 upgrade_flag(DONE/ERROR) →
//       ota_app_start() 创建 OTA 任务 → 创建 UART 命令任务 → 启动调度器。
//
// UART 命令协议（与 PC 侧脚本配合）：
//   I = 打印固件/标志状态；F = 下载固件到外部存储；R = 写 PENDING 并复位升级；
//   C = 触发一次 OTA 检查
//
// TODO: 按芯片实现时钟/GPIO 初始化（本模板只展示与 OTA 相关的调用顺序）。
// 参考：platforms/stm32f103/app/main.c
#include <stdint.h>
#include <stdbool.h>

// TODO: 芯片头文件（寄存器定义）
// #include "your_chip_header.h"

#include "board_uart.h"
#include "board_storage.h"
#include "printf_lite.h"   // 必须在 printf.h 之前：重定向 printf→printf_lite
#include "printf.h"        // LOG_* 宏
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "ota_main.h"      // ota_app_start() / g_ota_skip_initial_check / xEventQueue
#include "boot_protocol.h" // boot_read/write_upgrade_flag / UPGRADE_FLAG_*
#include "lzma/flow-unzip/unzip_stream.h"  // FirmwareHeader_t / FIRMWARE_MAGIC

// 固件下载分块大小（对齐存储页大小）
#define DL_CHUNK_SIZE  256
// 固件区最大允许大小
#define DL_MAX_SIZE    0x8000UL

// TODO: 可选 —— LED 心跳任务（用板级 GPIO 实现）
static void blink_task(void *arg) {
    (void)arg;
    for (;;) {
        // TODO: 翻转 LED 引脚
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// 'I' — 打印固件头 / upgrade_flag 状态
static void cmd_print_info(void) {
    FirmwareHeader_t hdr;
    board_storage_read(STORAGE_FW_OFFSET, (uint8_t *)&hdr, sizeof(hdr));
    if (hdr.magic == FIRMWARE_MAGIC) {
        printf("Firmware: comp=%u uncomp=%u ver=%s\n",
               (unsigned)hdr.compressed_size,
               (unsigned)hdr.uncompressed_size,
               hdr.version_str);
    } else {
        printf("Firmware: not present (magic=0x%08X)\n", (unsigned)hdr.magic);
    }
    uint32_t flag = boot_read_upgrade_flag();
    printf("Upgrade flag: 0x%08X\n", (unsigned)flag);
}

// 'F' — 通过 UART 下载固件到外部存储
// 协议：PC 发 'F' → 板回 "READY\n" → PC 发 4 字节 LE total_size →
//       板擦除回 "OK\n" → PC 分块发送，每块回 "ACK\n" → 完成回 "DONE\n"
static void cmd_firmware_download(void) {
    printf("READY\n");

    uint8_t sizebuf[4];
    for (int i = 0; i < 4; i++) {
        int c = board_uart_getc_timeout(5000000);
        if (c < 0) { printf("ERR: size timeout\n"); return; }
        sizebuf[i] = (uint8_t)c;
    }
    uint32_t total_size = (uint32_t)sizebuf[0]
                        | ((uint32_t)sizebuf[1] << 8)
                        | ((uint32_t)sizebuf[2] << 16)
                        | ((uint32_t)sizebuf[3] << 24);
    if (total_size == 0 || total_size > DL_MAX_SIZE) {
        printf("ERR: bad size %u\n", (unsigned)total_size);
        return;
    }

    printf("Erasing...\n");
    if (!board_storage_erase_range(STORAGE_FW_OFFSET, total_size)) {
        printf("ERR: erase failed\n");
        return;
    }
    printf("OK\n");

    uint8_t buf[DL_CHUNK_SIZE];
    uint32_t written = 0;
    while (written < total_size) {
        uint32_t chunk = total_size - written;
        if (chunk > DL_CHUNK_SIZE) chunk = DL_CHUNK_SIZE;
        for (uint32_t i = 0; i < chunk; i++) {
            int c = board_uart_getc_timeout(5000000);
            if (c < 0) {
                printf("ERR: data timeout at %u/%u\n",
                       (unsigned)written, (unsigned)total_size);
                return;
            }
            buf[i] = (uint8_t)c;
        }
        if (!board_storage_write(STORAGE_FW_OFFSET + written, buf, chunk)) {
            printf("ERR: write failed at %u\n", (unsigned)written);
            return;
        }
        written += chunk;
        printf("ACK %u/%u\n", (unsigned)written, (unsigned)total_size);
    }

    FirmwareHeader_t hdr;
    board_storage_read(STORAGE_FW_OFFSET, (uint8_t *)&hdr, sizeof(hdr));
    if (hdr.magic != FIRMWARE_MAGIC) {
        printf("ERR: bad magic 0x%08X\n", (unsigned)hdr.magic);
        return;
    }
    printf("DONE: comp=%u uncomp=%u ver=%s\n",
           (unsigned)hdr.compressed_size, (unsigned)hdr.uncompressed_size,
           hdr.version_str);
}

// 'R' — 写 PENDING 标志并软复位（Bootloader 接管升级）
static void cmd_trigger_upgrade(void) {
    printf("Writing PENDING flag...\n");
    if (!boot_write_upgrade_flag(UPGRADE_FLAG_PENDING)) {
        printf("ERR: write flag failed\n");
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(50));  // 等 UART 输出完成
    hal_ota_instance.system_reset();  // TODO: 也可直接写芯片复位寄存器
    while (1) { }
}

// UART 命令任务：轮询 RX 分发命令
static void cmd_task(void *arg) {
    (void)arg;
    for (;;) {
        int c = board_uart_getc_nonblock();
        if (c < 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        switch (c) {
            case 'I': case 'i': cmd_print_info(); break;
            case 'F': case 'f': cmd_firmware_download(); break;
            case 'R': case 'r': cmd_trigger_upgrade(); break;
            case 'C': case 'c': {
                ota_event_t event = EVENT_START_CHECK;
                xQueueSend(xEventQueue, &event, 0);
                printf("OTA check triggered\n");
                break;
            }
            case '?':
                printf("Commands: I=Info F=Flash R=Reset C=Check\n");
                break;
            default: break;
        }
    }
}

int main(void) {
    // ===== 1. TODO: 系统时钟初始化（如 SystemInit()） =====

    // ===== 2. 外设初始化 =====
    board_uart_init();
    board_storage_init();
    LOG_INFO(ICON_START " OTA App boot\n");

    // ===== 3. 检查 upgrade_flag（升级后首次启动处理） =====
    uint32_t flag = boot_read_upgrade_flag();
    if (flag == UPGRADE_FLAG_DONE) {
        LOG_SUCCESS("OTA 升级完成! 清除 DONE 标志\n");
        boot_write_upgrade_flag(UPGRADE_FLAG_NONE);
        g_ota_skip_initial_check = true;
    } else if (flag == UPGRADE_FLAG_ERROR) {
        LOG_ERROR("上次升级失败! flag=ERROR\n" RESET);
        g_ota_skip_initial_check = true;
    } else if (flag == UPGRADE_FLAG_PENDING) {
        LOG_ERROR("异常 PENDING 标志,清除\n" RESET);
        boot_write_upgrade_flag(UPGRADE_FLAG_NONE);
    }

    // ===== 4. 创建 OTA 任务并发起首次检查 =====
    ota_app_start();

    // ===== 5. 可选：LED 心跳 + UART 命令任务 =====
    xTaskCreate(blink_task, "blink", 128, NULL, 1, NULL);
    xTaskCreate(cmd_task,   "cmd",   512, NULL, 1, NULL);

    // ===== 6. 启动调度器 =====
    vTaskStartScheduler();
    for (;;) { }
    return 0;
}
