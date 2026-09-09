// firmware_source_spiflash.c —— SPI Flash 后端 firmware_source 实现
// 把 W25Qxx 包装成 firmware_source_t 接口,供 FSM/解压模块调用
#include "firmware_source.h"
#include "w25qxx.h"
#include <stdint.h>

// 固件镜像在 SPI Flash 中的起始偏移
#define SPI_FLASH_FW_OFFSET 0x000000

// 固件头大小(与 FirmwareHeader_t 一致,32 字节)
#define FW_HEADER_SIZE 32

typedef struct {
    uint32_t base;  // 固件起始偏移
} spiflash_ctx_t;

static spiflash_ctx_t g_spiflash_ctx = { .base = SPI_FLASH_FW_OFFSET };

// 从 SPI Flash 读取固件头前 12 字节(magic + version + compressed_size)
// 返回固件总大小(header + compressed),0=无效
static uint32_t spiflash_source_size(void *ctx) {
    spiflash_ctx_t *c = (spiflash_ctx_t *)ctx;
    uint8_t hdr[12];
    w25qxx_read(c->base, hdr, sizeof(hdr));
    // 小端序组装 magic
    uint32_t magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8)
                   | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
    if (magic != FIRMWARE_MAGIC) return 0;
    // compressed_size 在偏移 8(第 3 个 uint32_t)
    uint32_t compressed = (uint32_t)hdr[8] | ((uint32_t)hdr[9] << 8)
                        | ((uint32_t)hdr[10] << 16) | ((uint32_t)hdr[11] << 24);
    return FW_HEADER_SIZE + compressed;
}

// 从固件 offset 处读取 len 字节
static int spiflash_source_read(void *ctx, uint32_t offset, uint8_t *buf, uint32_t len) {
    spiflash_ctx_t *c = (spiflash_ctx_t *)ctx;
    w25qxx_read(c->base + offset, buf, len);
    return (int)len;
}

// 全局实例
firmware_source_t g_spiflash_source = {
    .ctx  = &g_spiflash_ctx,
    .size = spiflash_source_size,
    .read = spiflash_source_read,
};
