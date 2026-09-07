// firmware_source.h —— 固件源接口抽象
// 封装不同后端(PC 文件 / MCU SPI Flash)的固件读取操作
// 供 FSM 状态机和 LZMA 解压模块统一调用,消除文件依赖
#ifndef FIRMWARE_SOURCE_H
#define FIRMWARE_SOURCE_H

#include <stdint.h>

// 固件魔数 "FRWF"(与 unzip_stream.h 共享,guard 防重复定义)
#ifndef FIRMWARE_MAGIC
#define FIRMWARE_MAGIC 0x46575246
#endif

// 固件源接口:三字段结构体
typedef struct {
    void *ctx;        // 后端上下文(如 SPI Flash 基址)
    uint32_t (*size)(void *ctx);                           // 返回固件总大小(字节),0=不存在
    int (*read)(void *ctx, uint32_t offset, uint8_t *buf, uint32_t len); // 从 offset 读 len 字节,返回读取字节数
} firmware_source_t;

// MCU SPI Flash 后端实例(定义在 firmware_source_spiflash.c)
extern firmware_source_t g_spiflash_source;

#endif // FIRMWARE_SOURCE_H
