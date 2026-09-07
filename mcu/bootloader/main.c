// main.c —— STM32F103C8T6 Bootloader 入口(无 RTOS,16KB @ 0x08000000)
//
// 流程:
//   1. SystemInit + 外设初始化(RCC/GPIO/UART/SPI/W25Q 探针)
//   2. 读 SPI Flash 元数据区 upgrade_flag
//   3. 若 == PENDING:擦 App 区 → 从 SPI Flash 流式 LZMA 解压写内部 Flash
//                       → 清 upgrade_flag(DONE)
//   4. 跳转 App @0x08004000(MSP + VTOR + reset vector)
//
// 依赖:stm32f1_flash.c(擦写内部 Flash)、w25qxx.c(读 SPI Flash +
//   write upgrade_flag)、lzma_alloc.c(静态 LZMA 内存)、
//   firmware_source_spiflash.c(固件源)、unzip_stream.c(流式解压)
#include "stm32f103xb.h"
#include "stm32f1_gpio.h"
#include "stm32f1_uart.h"
#include "stm32f1_spi.h"
#include "w25qxx.h"
#include "stm32f1_flash.h"
#include "firmware_source.h"
#include "unzip_stream.h"
#include "boot_protocol.h"
#include "printf_lite.h"

// Cortex-M3 跳转到 App
static void jump_to_app(void) {
    uint32_t app_msp   = *(volatile uint32_t *)APP_BASE_ADDR;
    uint32_t app_reset = *(volatile uint32_t *)(APP_BASE_ADDR + 4);

    // 关中断
    __asm volatile ("cpsid i" ::: "memory");

    // 重映射向量表到 App 区
    SCB->VTOR = APP_BASE_ADDR;

    // 设 MSP
    __asm volatile ("msr msp, %0" :: "r"(app_msp) : "memory");

    // 跳到 App reset handler
    ((void (*)(void))app_reset)();

    // 不会返回
    while (1) { }
}

// Bootloader 内部 Flash 写入上下文:维护当前写入地址 + 4 字节 tail 缓冲
//   LZMA 解码输出块大小可能不是 4 字节倍数,tail 缓冲凑齐后再写 word
typedef struct {
    uint32_t addr;       // 下一个写入地址(4 字节对齐)
    uint8_t  tail[4];    // 不足 4 字节的尾部缓冲
    uint32_t tail_len;   // tail 中已积攒字节数(0..3)
    bool     error;      // 写入失败标志
} flash_write_ctx_t;

static flash_write_ctx_t g_flash_ctx;

// 初始化 Flash 写入上下文:从 App 起始地址开始
static void flash_write_ctx_reset(void) {
    g_flash_ctx.addr     = APP_BASE_ADDR;
    g_flash_ctx.tail_len = 0;
    g_flash_ctx.error    = false;
}

// flush tail:把不足 4 字节的 tail 补 0xFF 后写一个 word(Flash 擦后是 0xFF,
// 补 0xFF 不改变 Flash 已擦状态,语义安全)
static void flash_write_ctx_flush_tail(void) {
    if (g_flash_ctx.tail_len == 0) return;
    // 补 0xFF 到 4 字节
    for (uint32_t i = g_flash_ctx.tail_len; i < 4; i++) {
        g_flash_ctx.tail[i] = 0xFF;
    }
    if (!flash_write_word(g_flash_ctx.addr, *(uint32_t *)g_flash_ctx.tail)) {
        g_flash_ctx.error = true;
    }
    g_flash_ctx.addr    += 4;
    g_flash_ctx.tail_len = 0;
}

// 解压输出回调:把 data[0..size) 写入内部 Flash
// 内部用 tail 缓冲保证每次 flash_write_word 都是 4 字节对齐
static int flash_output_write_cb(void *write_user, const uint8_t *data, size_t size) {
    (void)write_user;
    if (g_flash_ctx.error) return -1;

    // 越界检查:防止写入超出 App 区
    if (g_flash_ctx.addr + g_flash_ctx.tail_len + size > APP_END_ADDR) {
        printf("错误:Flash 写入越界 (addr=0x%08X, size=%u)\n",
               (unsigned)g_flash_ctx.addr, (unsigned)size);
        g_flash_ctx.error = true;
        return -1;
    }

    size_t i = 0;
    // 先把 tail 凑齐 4 字节
    while (g_flash_ctx.tail_len > 0 && i < size) {
        g_flash_ctx.tail[g_flash_ctx.tail_len++] = data[i++];
        if (g_flash_ctx.tail_len == 4) {
            if (!flash_write_word(g_flash_ctx.addr,
                                  *(uint32_t *)g_flash_ctx.tail)) {
                g_flash_ctx.error = true;
                return -1;
            }
            g_flash_ctx.addr    += 4;
            g_flash_ctx.tail_len = 0;
        }
    }
    // 整块 4 字节倍数直接写
    while (i + 4 <= size) {
        uint32_t word = (uint32_t)data[i]
                      | ((uint32_t)data[i+1] << 8)
                      | ((uint32_t)data[i+2] << 16)
                      | ((uint32_t)data[i+3] << 24);
        if (!flash_write_word(g_flash_ctx.addr, word)) {
            g_flash_ctx.error = true;
            return -1;
        }
        g_flash_ctx.addr += 4;
        i += 4;
    }
    // 剩余 < 4 字节入 tail
    while (i < size) {
        g_flash_ctx.tail[g_flash_ctx.tail_len++] = data[i++];
    }
    return 0;
}

// 升级流程:从 SPI Flash 解压固件写入 App 区
// 成功返回 true,失败返回 false
static bool perform_upgrade(void) {
    printf("\n==== 开始 OTA 升级 ====\n");

    // 1. 解锁并擦除 App 区(48KB,48 页 × 1KB)
    printf("[1/4] 擦除 App 区 (0x%08X, %u KB)...\n",
           (unsigned)APP_BASE_ADDR, (unsigned)(APP_MAX_SIZE / 1024));
    flash_unlock();
    if (!flash_erase_range(APP_BASE_ADDR, APP_MAX_SIZE)) {
        printf("错误:擦除 App 区失败\n");
        flash_lock();
        return false;
    }
    printf("  App 区擦除完成\n");

    // 2. 流式 LZMA 解压 + 写 Flash
    printf("[2/4] 流式解压固件并写入 App 区...\n");
    flash_write_ctx_reset();
    uint32_t out_crc = 0;
    uint64_t out_total = 0;
    int ret = perform_firmware_update_from_source(&g_spiflash_source,
                                                  flash_output_write_cb, NULL,
                                                  &out_crc, &out_total);
    // 刷出最后不足 4 字节的 tail
    flash_write_ctx_flush_tail();
    flash_lock();

    if (ret != 0 || g_flash_ctx.error) {
        printf("错误:固件写入失败 (ret=%d, ctx_err=%d)\n",
               ret, (int)g_flash_ctx.error);
        return false;
    }
    printf("  固件写入完成: %lu 字节, CRC=0x%08X\n",
           (unsigned long)out_total, out_crc);

    // 3. 简单校验 App 区头 4 字节(应为 App 的 MSP,非 0xFFFFFFFF)
    printf("[3/4] 校验 App 区起始...\n");
    uint32_t app_msp = *(volatile uint32_t *)APP_BASE_ADDR;
    if (app_msp == 0xFFFFFFFF || app_msp == 0x00000000) {
        printf("错误:App 区起始无效 (MSP=0x%08X)\n", app_msp);
        return false;
    }
    printf("  App MSP=0x%08X (有效)\n", app_msp);

    // 4. 写 DONE 标志
    printf("[4/4] 清 upgrade_flag -> DONE...\n");
    if (!boot_write_upgrade_flag(UPGRADE_FLAG_DONE)) {
        printf("警告:清除标志位失败(下次启动可能重复升级)\n");
    }

    printf("==== OTA 升级成功!====\n\n");
    return true;
}

int main(void) {
    // 1. 时钟 + 外设初始化
    SystemInit();           // cmsis/system_stm32f1xx.c:72MHz
    uart1_init();           // UART1 @115200(GPIOA 时钟由内部开)
    spi1_init();            // SPI1(GPIOA 时钟由内部开)

    printf("\n\n*** STM32F103 Bootloader 启动 ***\n");
    printf("  App 区: 0x%08X - 0x%08X (%u KB)\n",
           (unsigned)APP_BASE_ADDR, (unsigned)APP_END_ADDR,
           (unsigned)(APP_MAX_SIZE / 1024));

    // 2. W25Q 探针
    uint8_t mfr = 0, type = 0, cap = 0;
    w25qxx_read_id(&mfr, &type, &cap);
    printf("  SPI Flash ID: 0x%02X 0x%02X 0x%02X\n", mfr, type, cap);
    if (mfr == 0xEF && (type == 0x40 || type == 0x60)) {
        printf("  检测到 Winbond W25Qxx\n");
    } else if (mfr == 0x00 || mfr == 0xFF) {
        printf("  警告:未检测到 SPI Flash,跳过升级直接跳转 App\n");
        jump_to_app();
    }

    // 3. 读 upgrade_flag
    uint32_t flag = boot_read_upgrade_flag();
    printf("  upgrade_flag = 0x%08X\n", (unsigned)flag);

    if (flag == UPGRADE_FLAG_PENDING) {
        printf("  检测到待升级标志,进入 OTA 流程\n");
        bool ok = perform_upgrade();
        if (!ok) {
            printf("  OTA 失败,写 ERROR 标志后跳转 App(若有)\n");
            boot_write_upgrade_flag(UPGRADE_FLAG_ERROR);
            // 失败时 App 区可能已损坏,仍尝试跳转(可能死机,需手动恢复)
        }
    } else {
        printf("  无升级标志,直接跳转 App\n");
    }

    // 4. 跳转 App
    printf(">>> 跳转 App @ 0x%08X <<<\n", (unsigned)APP_BASE_ADDR);
    jump_to_app();

    // 不应到达
    while (1) { }
}
