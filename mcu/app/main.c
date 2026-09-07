// mcu/app/main.c —— STM32F103C8T6 App 固件入口
// P4: 硬件初始化(RCC/GPIO/UART/SPI Flash) → 调用 ota_app_start() 创建 OTA 任务
// P7: 新增 UART 命令接口(I=信息/F=固件下载/R=触发升级/C=OTA检查)
//      + 启动时检测 DONE 标志(升级后首次启动清除并跳过自动 OTA 检查)
// 注意：FreeRTOS 钩子(vAssertCalled/vApplicationMallocFailedHook/
//       vApplicationStackOverflowHook)由 OTA-Update/app/ota_main.c 提供(MCU 路径)
#include "stm32f103xb.h"
#include "stm32f1_gpio.h"
#include "stm32f1_uart.h"
#include "stm32f1_spi.h"
#include "w25qxx.h"
#include "printf_lite.h"  // 必须在 printf.h 之前:重定向 printf→printf_lite
#include "printf.h"        // LOG_* 宏
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "ota_main.h"      // ota_app_start() / g_ota_skip_initial_check
#include "boot_protocol.h"  // boot_read/write_upgrade_flag / UPGRADE_FLAG_*
#include "lzma/flow-unzip/unzip_stream.h"  // FirmwareHeader_t / FIRMWARE_MAGIC

// 固件下载分块大小(对齐 SPI Flash 页大小 256B)
#define DL_CHUNK_SIZE  256
// 固件区最大允许大小(保守 32KB,W25Q80 1MB 的 1/32)
#define DL_MAX_SIZE    0x8000UL

// LED blink 任务：直观指示调度器在跑
static void blink_task(void *arg) {
    (void)arg;
    for (;;) {
        gpio_toggle(GPIOC, 13);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ==========================================
// P7: UART 命令实现
// ==========================================

// 'I' — 打印 SPI Flash / 固件头 / upgrade_flag 状态
static void cmd_print_info(void) {
    uint8_t mid, mtype, mcap;
    w25qxx_read_id(&mid, &mtype, &mcap);
    printf("SPI Flash: %02X %02X %02X\n", mid, mtype, mcap);

    FirmwareHeader_t hdr;
    w25qxx_read(SPI_FLASH_FW_OFFSET, (uint8_t *)&hdr, sizeof(hdr));
    if (hdr.magic == FIRMWARE_MAGIC) {
        printf("Firmware: comp=%u uncomp=%u ver=%s\n",
               (unsigned)hdr.compressed_size,
               (unsigned)hdr.uncompressed_size,
               hdr.version_str);
    } else {
        printf("Firmware: not present (magic=0x%08X)\n",
               (unsigned)hdr.magic);
    }

    uint32_t flag = boot_read_upgrade_flag();
    printf("Upgrade flag: 0x%08X", (unsigned)flag);
    if (flag == UPGRADE_FLAG_PENDING)      printf(" (PENDING)");
    else if (flag == UPGRADE_FLAG_DONE)    printf(" (DONE)");
    else if (flag == UPGRADE_FLAG_ERROR)   printf(" (ERROR)");
    printf("\n");
}

// 'F' — 通过 UART 下载固件到 SPI Flash
// 协议:
//   PC 发 'F' → App 回 "READY\n"
//   PC 发 4 字节 LE total_size → App 擦除 + 回 "OK\n"
//   PC 分 256B 块发送,每块后 App 回 "ACK\n"
//   全部完成后 App 校验固件头并回 "DONE\n" / "ERR: ...\n"
static void cmd_firmware_download(void) {
    printf("READY\n");

    // 读取 4 字节 LE total_size(每字节超时 ~35ms)
    uint8_t sizebuf[4];
    for (int i = 0; i < 4; i++) {
        int c = uart1_getc_timeout(5000000);
        if (c < 0) {
            printf("ERR: size timeout\n");
            return;
        }
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
    printf("Size: %u bytes\n", (unsigned)total_size);

    // 擦除 SPI Flash 固件区
    printf("Erasing...\n");
    if (!w25qxx_erase_range(SPI_FLASH_FW_OFFSET, total_size)) {
        printf("ERR: erase failed\n");
        return;
    }
    printf("OK\n");

    // 分块接收 + 写入
    uint8_t buf[DL_CHUNK_SIZE];
    uint32_t written = 0;
    while (written < total_size) {
        uint32_t chunk = total_size - written;
        if (chunk > DL_CHUNK_SIZE) chunk = DL_CHUNK_SIZE;

        for (uint32_t i = 0; i < chunk; i++) {
            int c = uart1_getc_timeout(5000000);
            if (c < 0) {
                printf("ERR: data timeout at %u/%u\n",
                       (unsigned)written, (unsigned)total_size);
                return;
            }
            buf[i] = (uint8_t)c;
        }

        if (!w25qxx_write(SPI_FLASH_FW_OFFSET + written, buf, chunk)) {
            printf("ERR: write failed at %u\n", (unsigned)written);
            return;
        }
        written += chunk;
        printf("ACK %u/%u\n", (unsigned)written, (unsigned)total_size);
    }

    // 校验固件头
    FirmwareHeader_t hdr;
    w25qxx_read(SPI_FLASH_FW_OFFSET, (uint8_t *)&hdr, sizeof(hdr));
    if (hdr.magic != FIRMWARE_MAGIC) {
        printf("ERR: bad magic 0x%08X\n", (unsigned)hdr.magic);
        return;
    }
    printf("DONE: comp=%u uncomp=%u ver=%s\n",
           (unsigned)hdr.compressed_size,
           (unsigned)hdr.uncompressed_size,
           hdr.version_str);
}

// 'R' — 快速触发升级:写 PENDING 标志 + 软复位
static void cmd_trigger_upgrade(void) {
    printf("Writing PENDING flag...\n");
    if (!boot_write_upgrade_flag(UPGRADE_FLAG_PENDING)) {
        printf("ERR: write flag failed\n");
        return;
    }
    printf("PENDING set. Resetting...\n");
    vTaskDelay(pdMS_TO_TICKS(50));  // 等 UART 输出完成
    // 触发 Cortex-M3 软复位
    __asm volatile ("cpsid i" ::: "memory");
    SCB->AIRCR = (0x5FA << 16) | (1u << 2);  // VECTKEY + SYSRESETREQ
    while (1) { }
}

// UART 命令任务:轮询 RX,分发命令
static void cmd_task(void *arg) {
    (void)arg;
    for (;;) {
        int c = uart1_getc_nonblock();
        if (c < 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        switch (c) {
            case 'I': case 'i':
                cmd_print_info();
                break;
            case 'F': case 'f':
                cmd_firmware_download();
                break;
            case 'R': case 'r':
                cmd_trigger_upgrade();
                break;
            case 'C': case 'c':
                {
                    ota_event_t event = EVENT_START_CHECK;
                    xQueueSend(xEventQueue, &event, 0);
                    printf("OTA check triggered\n");
                }
                break;
            case '?':
                printf("Commands: I=Info F=Flash R=Reset C=Check\n");
                break;
            default:
                break;  // 忽略换行/回车/未知字符
        }
    }
}

int main(void) {
    // ===== 1. 时钟门控 + GPIO/UART 初始化 =====
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
    gpio_set_mode(GPIOC, 13, GPIO_MODE_OUTPUT_PP_50);  // 板载 LED
    uart1_init();
    LOG_INFO(ICON_START " STM32F103C8T6 P7 OTA + FreeRTOS boot\n");

    // ===== 2. SPI Flash 初始化 + 固件源探针 =====
    spi1_init();
    uint8_t mid, mtype, mcap;
    w25qxx_read_id(&mid, &mtype, &mcap);
    LOG_INFO("W25Q ID: %02X %02X %02X\n",
             (unsigned)mid, (unsigned)mtype, (unsigned)mcap);

    // ===== 3. 检查 upgrade_flag(升级后首次启动清除并跳过自动检查) =====
    uint32_t flag = boot_read_upgrade_flag();
    if (flag == UPGRADE_FLAG_DONE) {
        LOG_SUCCESS("OTA 升级完成! 清除 DONE 标志\n");
        boot_write_upgrade_flag(UPGRADE_FLAG_NONE);
        g_ota_skip_initial_check = true;
    } else if (flag == UPGRADE_FLAG_ERROR) {
        LOG_ERROR("上次升级失败! flag=ERROR\n" RESET);
        g_ota_skip_initial_check = true;
    } else if (flag == UPGRADE_FLAG_PENDING) {
        // PENDING 在 App 区不应出现(Bootloader 应已处理),清除并继续
        LOG_ERROR("异常 PENDING 标志,清除\n" RESET);
        boot_write_upgrade_flag(UPGRADE_FLAG_NONE);
    }

    // ===== 4. 创建 OTA 任务并发起首次更新检查 =====
    ota_app_start();

    // ===== 5. LED blink + UART 命令任务(低优先级) =====
    xTaskCreate(blink_task, "blink", 128, NULL, 1, NULL);
    xTaskCreate(cmd_task,   "cmd",   512, NULL, 1, NULL);

    // ===== 6. 启动调度器 =====
    vTaskStartScheduler();
    for (;;) { }  // 不可达
    return 0;
}
