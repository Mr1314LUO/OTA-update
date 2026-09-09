// firmware_source_impl.c —— 平台 firmware_source_t 实例（模板）
//
// 把外部存储（board_storage）包装成 lib/hal/firmware_source.h 定义的固件源接口，
// 供 FSM 状态机与 LZMA 流式解压统一调用。
//
// 提供 lib/app/ota_main.c 与 bootloader/main.c 引用的全局符号 g_spiflash_source
// （符号名固定，勿改）。
//
// 参考：platforms/stm32f103/drivers/firmware_source_spiflash.c
#include "firmware_source.h"
#include "board_storage.h"
#include <stdint.h>

// 固件镜像在外部存储中的起始偏移（与 app/main.c 下载写入位置一致）
#define FW_STORAGE_OFFSET  0x000000UL

// 固件头大小（与 FirmwareHeader_t 一致，32 字节）
#define FW_HEADER_SIZE     32

typedef struct {
    uint32_t base;   // 固件起始偏移
} fw_ctx_t;

static fw_ctx_t g_fw_ctx = { .base = FW_STORAGE_OFFSET };

// 返回固件总大小（header + compressed），0 = 无有效固件
// 读固件头前 12 字节：magic(4) + version(4) + compressed_size(4)，小端
static uint32_t fw_source_size(void *ctx) {
    fw_ctx_t *c = (fw_ctx_t *)ctx;
    uint8_t hdr[12];
    board_storage_read(c->base, hdr, sizeof(hdr));

    uint32_t magic = (uint32_t)hdr[0]       | ((uint32_t)hdr[1] << 8)
                   | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
    if (magic != FIRMWARE_MAGIC) return 0;

    uint32_t compressed = (uint32_t)hdr[8]        | ((uint32_t)hdr[9] << 8)
                        | ((uint32_t)hdr[10] << 16) | ((uint32_t)hdr[11] << 24);
    return FW_HEADER_SIZE + compressed;
}

// 从固件 offset 处读取 len 字节，返回读取字节数
static int fw_source_read(void *ctx, uint32_t offset, uint8_t *buf, uint32_t len) {
    fw_ctx_t *c = (fw_ctx_t *)ctx;
    board_storage_read(c->base + offset, buf, len);
    return (int)len;
}

// 全局实例 —— 符号名必须为 g_spiflash_source（lib/app/ota_main.c 直接引用）
firmware_source_t g_spiflash_source = {
    .ctx  = &g_fw_ctx,
    .size = fw_source_size,
    .read = fw_source_read,
};
