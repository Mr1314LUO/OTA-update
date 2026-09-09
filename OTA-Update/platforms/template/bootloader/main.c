// main.c —— Bootloader 固件入口（模板，裸机无 RTOS）
//
// 流程：
//   1. 时钟 + 外设初始化(UART/外部存储/内部 Flash)
//   2. 读 upgrade_flag；若 == PENDING：
//        擦 App 区 → 从外部存储流式 LZMA 解压写内部 Flash → 写 DONE 标志
//   3. 跳转 App（MSP + 复位向量）
//
// TODO: 按架构实现 jump_to_app() 与内部 Flash 编程细节。
// 参考：platforms/stm32f103/bootloader/main.c
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// TODO: 芯片头文件
// #include "your_chip_header.h"

#include "board_uart.h"
#include "board_storage.h"
#include "board_flash.h"
#include "firmware_source.h"       // firmware_source_t / g_spiflash_source
#include "unzip_stream.h"          // perform_firmware_update_from_source()
#include "boot_protocol.h"
#include "printf_lite.h"

// ==========================================
// 跳转到 App（Cortex-M 实现；其他架构需改写）
// ==========================================
// TODO: 非 Cortex-M 架构按 ABI 实现跳转（设栈指针 + 跳复位地址）
static void jump_to_app(void) {
#if 1   // Cortex-M 版本：MSP 在向量表[0]，复位向量在向量表[1]
    uint32_t app_msp   = *(volatile uint32_t *)APP_BASE_ADDR;
    uint32_t app_reset = *(volatile uint32_t *)(APP_BASE_ADDR + 4);

    __asm volatile ("cpsid i" ::: "memory");          // 关中断
    // SCB->VTOR = APP_BASE_ADDR;                       // 重定位向量表
    __asm volatile ("msr msp, %0" :: "r"(app_msp) : "memory");  // 设 MSP
    ((void (*)(void))app_reset)();
    while (1) { }
#else
    // 其他架构：在此实现等价跳转
    while (1) { }
#endif
}

// ==========================================
// 解压输出 → 内部 Flash 写入
// tail 缓冲保证每次写都是 Flash 最小编程单位（Cortex-M STM32 为 4 字节 word）。
// TODO: 若芯片最小编程单位不是 4 字节，修改 TAIL_UNIT。
// ==========================================
#define TAIL_UNIT 4

typedef struct {
    uint32_t addr;       // 下一个写入地址
    uint8_t  tail[TAIL_UNIT];
    uint32_t tail_len;
    bool     error;
} flash_write_ctx_t;

static flash_write_ctx_t g_flash_ctx;

static void flash_ctx_reset(void) {
    g_flash_ctx.addr     = APP_BASE_ADDR;
    g_flash_ctx.tail_len = 0;
    g_flash_ctx.error    = false;
}

static void flash_ctx_flush_tail(void) {
    if (g_flash_ctx.tail_len == 0) return;
    // 补 0xFF 到编程单位边界（擦除态为 0xFF，补齐语义安全）
    for (uint32_t i = g_flash_ctx.tail_len; i < TAIL_UNIT; i++) {
        g_flash_ctx.tail[i] = 0xFF;
    }
    if (!board_flash_write_word(g_flash_ctx.addr,
                                *(uint32_t *)g_flash_ctx.tail)) {
        g_flash_ctx.error = true;
    }
    g_flash_ctx.addr    += TAIL_UNIT;
    g_flash_ctx.tail_len = 0;
}

static int flash_output_write_cb(void *write_user, const uint8_t *data, size_t size) {
    (void)write_user;
    if (g_flash_ctx.error) return -1;

    if (g_flash_ctx.addr + g_flash_ctx.tail_len + size > APP_END_ADDR) {
        g_flash_ctx.error = true;
        return -1;
    }

    size_t i = 0;
    while (g_flash_ctx.tail_len > 0 && i < size) {
        g_flash_ctx.tail[g_flash_ctx.tail_len++] = data[i++];
        if (g_flash_ctx.tail_len == TAIL_UNIT) {
            if (!board_flash_write_word(g_flash_ctx.addr,
                                        *(uint32_t *)g_flash_ctx.tail)) {
                g_flash_ctx.error = true;
                return -1;
            }
            g_flash_ctx.addr    += TAIL_UNIT;
            g_flash_ctx.tail_len = 0;
        }
    }
    while (i + TAIL_UNIT <= size) {
        uint32_t word = (uint32_t)data[i]
                      | ((uint32_t)data[i+1] << 8)
                      | ((uint32_t)data[i+2] << 16)
                      | ((uint32_t)data[i+3] << 24);
        if (!board_flash_write_word(g_flash_ctx.addr, word)) {
            g_flash_ctx.error = true;
            return -1;
        }
        g_flash_ctx.addr += TAIL_UNIT;
        i += TAIL_UNIT;
    }
    while (i < size) {
        g_flash_ctx.tail[g_flash_ctx.tail_len++] = data[i++];
    }
    return 0;
}

// 升级流程：从外部存储解压固件写入 App 区
static bool perform_upgrade(void) {
    printf("\n==== 开始 OTA 升级 ====\n");

    printf("[1/4] 擦除 App 区 (0x%08X, %u KB)...\n",
           (unsigned)APP_BASE_ADDR, (unsigned)(APP_MAX_SIZE / 1024));
    board_flash_unlock();
    if (!board_flash_erase_range(APP_BASE_ADDR, APP_MAX_SIZE)) {
        printf("错误:擦除 App 区失败\n");
        board_flash_lock();
        return false;
    }

    printf("[2/4] 流式解压固件并写入 App 区...\n");
    flash_ctx_reset();
    uint32_t out_crc = 0;
    uint64_t out_total = 0;
    int ret = perform_firmware_update_from_source(&g_spiflash_source,
                                                  flash_output_write_cb, NULL,
                                                  &out_crc, &out_total);
    flash_ctx_flush_tail();
    board_flash_lock();
    if (ret != 0 || g_flash_ctx.error) {
        printf("错误:固件写入失败 (ret=%d)\n", ret);
        return false;
    }
    printf("  固件写入完成: %lu 字节, CRC=0x%08X\n",
           (unsigned long)out_total, out_crc);

    printf("[3/4] 校验 App 区起始...\n");
    uint32_t app_msp = *(volatile uint32_t *)APP_BASE_ADDR;
    if (app_msp == 0xFFFFFFFF || app_msp == 0x00000000) {
        printf("错误:App 区起始无效 (MSP=0x%08X)\n", app_msp);
        return false;
    }

    printf("[4/4] 写 DONE 标志...\n");
    boot_write_upgrade_flag(UPGRADE_FLAG_DONE);
    printf("==== OTA 升级成功 ====\n\n");
    return true;
}

int main(void) {
    // 1. TODO: 系统时钟初始化
    board_uart_init();
    board_storage_init();

    printf("\n\n*** Bootloader 启动 ***\n");
    printf("  App 区: 0x%08X - 0x%08X (%u KB)\n",
           (unsigned)APP_BASE_ADDR, (unsigned)APP_END_ADDR,
           (unsigned)(APP_MAX_SIZE / 1024));

    // 2. 读 upgrade_flag
    uint32_t flag = boot_read_upgrade_flag();
    printf("  upgrade_flag = 0x%08X\n", (unsigned)flag);

    if (flag == UPGRADE_FLAG_PENDING) {
        if (!perform_upgrade()) {
            boot_write_upgrade_flag(UPGRADE_FLAG_ERROR);
        }
    } else {
        printf("  无升级标志,直接跳转 App\n");
    }

    // 3. 跳转 App
    printf(">>> 跳转 App @ 0x%08X <<<\n", (unsigned)APP_BASE_ADDR);
    jump_to_app();
    while (1) { }
}
