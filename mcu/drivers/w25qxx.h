// w25qxx.h —— W25Qxx SPI Flash 驱动(读 + 写 + 擦)
#ifndef W25QXX_H
#define W25QXX_H
#include <stdint.h>
#include <stdbool.h>

// 读取 JEDEC ID(制造商/类型/容量)
void w25qxx_read_id(uint8_t *manufacturer, uint8_t *memory_type, uint8_t *capacity);

// 从 addr 读取 len 字节到 buf
void w25qxx_read(uint32_t addr, uint8_t *buf, uint32_t len);

// === 写操作相关(P6 新增) ===

// 扇区大小(4KB),页大小(256B)
#define W25Q_SECTOR_SIZE  4096
#define W25Q_PAGE_SIZE    256

// 读状态寄存器(主要用于 BUSY 位检测)
uint8_t w25qxx_read_status(void);

// 等待 Flash 空闲(BUSY=0),超时返回 false
bool w25qxx_wait_idle(uint32_t timeout_ms);

// 发写使能命令(WEL 置位,擦/写前必须调用)
void w25qxx_write_enable(void);

// 擦除 4KB 扇区(addr 会被自动扇区对齐到 4KB 边界)
// 内部:WE → 等空闲 → SECTOR_ERASE → 等空闲
bool w25qxx_erase_sector(uint32_t addr);

// 页编程:向 addr 写入 len 字节(len<=256 且不跨页边界)
// 调用前应已擦除对应区域;内部自动 WE + 等空闲 + PP + 等空闲
bool w25qxx_program_page(uint32_t addr, const uint8_t *data, uint32_t len);

// 通用写:自动按 256B 拆分页编程(不擦除,调用者负责先擦)
bool w25qxx_write(uint32_t addr, const uint8_t *data, uint32_t len);

// 先擦对应扇区再写(仅适用于数据完全落在单个 4KB 扇区内的小块写)
// 用于元数据/标志位回写场景
bool w25qxx_write_with_erase(uint32_t addr, const uint8_t *data, uint32_t len);

// === P7: 批量擦除 ===

// 擦除覆盖 [addr, addr+len) 的所有 4KB 扇区
// 用于固件下载前批量擦除固件区
bool w25qxx_erase_range(uint32_t addr, uint32_t len);

#endif
