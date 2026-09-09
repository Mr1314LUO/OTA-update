// board_storage.h —— 外部存储驱动契约（模板）
//
// 用途：存放 OTA 固件镜像（FirmwareHeader_t + LZMA 压缩数据）与 upgrade_flag。
//   stm32 参考实现用 W25Qxx SPI Flash；你的芯片可用任意介质
//   （SPI Flash / 内部 Flash 另一扇区 / eMMC / SD 卡……），只要实现下列接口。
// 参考：platforms/stm32f103/drivers/w25qxx.c
//
// 地址模型：线性字节地址空间，从 0 开始。
//   [0, STORAGE_FW_MAX)            —— 固件镜像区
//   [STORAGE_FLAG_OFFSET, +4KB)    —— upgrade_flag 标志扇区（见 boot_protocol.h）
#ifndef BOARD_STORAGE_H
#define BOARD_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

// 介质几何参数（按你的芯片/外部芯片修改）
#define BOARD_STORAGE_SECTOR_SIZE  4096   // 擦除扇区大小（SPI Flash 典型 4KB）
#define BOARD_STORAGE_PAGE_SIZE    256    // 页编程大小

// 初始化存储接口（SPI 主机 + GPIO 片选等）
void board_storage_init(void);

// 从 addr 读取 len 字节到 buf（任意地址/长度，无需对齐）
void board_storage_read(uint32_t addr, uint8_t *buf, uint32_t len);

// 写入：自动按页拆分编程（不擦除；调用者需先擦除对应区域）
// 成功返回 true
bool board_storage_write(uint32_t addr, const uint8_t *data, uint32_t len);

// 擦除覆盖 [addr, addr+len) 的所有扇区（addr 自动扇区对齐，len 向上取整）
// 用于固件下载前清空固件区
bool board_storage_erase_range(uint32_t addr, uint32_t len);

// 先擦 addr 所在扇区再写（仅用于数据完全落在单个扇区内的小块写，如 upgrade_flag）
bool board_storage_write_with_erase(uint32_t addr, const uint8_t *data, uint32_t len);

#endif // BOARD_STORAGE_H
