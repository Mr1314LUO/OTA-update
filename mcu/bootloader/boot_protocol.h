// boot_protocol.h —— App 与 Bootloader 共享的 OTA 升级协议
// 约定 SPI Flash 元数据/固件区布局 + upgrade_flag 标志位
#ifndef BOOT_PROTOCOL_H
#define BOOT_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

// === App 区 / Boot 区 地址(与链接脚本一致) ===
#define BOOT_BASE_ADDR    0x08000000UL  // Bootloader 起始
#define APP_BASE_ADDR     0x08004000UL  // App 起始(16KB 之后)
#define APP_MAX_SIZE      0x0000C000UL  // App 区大小(48KB)
#define APP_END_ADDR      (APP_BASE_ADDR + APP_MAX_SIZE)

// === SPI Flash 布局 ===
// 固件镜像区(从偏移 0 开始,与 firmware_source_spiflash.c 一致)
//   [0 .. 固件大小) — FirmwareHeader_t + LZMA props + 压缩数据
#define SPI_FLASH_FW_OFFSET    0x000000UL

// 元数据区(W25Q80 1MB 末尾 4KB 扇区,用于 upgrade_flag)
//   注:若芯片容量不同,改此宏即可(须为 4KB 扇区对齐)
#ifndef SPI_FLASH_FLAG_OFFSET
#define SPI_FLASH_FLAG_OFFSET  0x0FF000UL
#endif

// === upgrade_flag 标志值(4 字节 magic,写 SPI Flash 元数据区) ===
// App 下载完固件后写 PENDING,复位;Bootloader 看到 PENDING 执行升级,
// 完成后写 DONE,然后跳 App;App 启动后可读 DONE 自检升级成功
#define UPGRADE_FLAG_NONE      0x00000000UL  // 空闲(擦后默认 0xFF,不会等于)
#define UPGRADE_FLAG_PENDING   0x4F544150UL  // "OTAP" ASCII — 待升级
#define UPGRADE_FLAG_DONE      0x4F544144UL  // "OTAD" ASCII — 升级完成
#define UPGRADE_FLAG_ERROR     0x4F544145UL  // "OTAE" ASCII — 升级失败

// 从 SPI Flash 读取 upgrade_flag(4 字节小端)
// 返回 magic 值(NONE/PENDING/DONE/ERROR,或 0xFFFFFFFF 表示未擦)
uint32_t boot_read_upgrade_flag(void);

// 写 upgrade_flag(需先擦对应扇区)
// 成功返回 true
bool boot_write_upgrade_flag(uint32_t magic);

#endif
