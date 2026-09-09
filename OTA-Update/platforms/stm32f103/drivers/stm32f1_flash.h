// stm32f1_flash.h —— STM32F1 内部 Flash 擦写驱动(裸寄存器)
// 用于 Bootloader 擦写 App 区(0x08004000~0x0800FFFF)
#ifndef STM32F1_FLASH_H
#define STM32F1_FLASH_H
#include <stdint.h>
#include <stdbool.h>

// STM32F103 中容量页大小(1KB)
#define STM32F1_FLASH_PAGE_SIZE 0x400

// 解锁 Flash 控制器(写入 KEYR 序列),若已解锁则跳过
void flash_unlock(void);

// 锁定 Flash 控制器
void flash_lock(void);

// 等待 Flash 不忙(BSY=0),超时返回 false
bool flash_wait_idle(uint32_t timeout_ms);

// 擦除单个页(addr 会被自动页对齐到 1KB 边界)
// 成功返回 true,失败(超时/错误)返回 false
bool flash_erase_page(uint32_t addr);

// 擦除地址范围 [start, start+len) 覆盖的所有页(每页 1KB)
// start 会被自动页对齐;len 向上取整到页边界
bool flash_erase_range(uint32_t start, uint32_t len);

// 字编程(4 字节对齐地址):向 addr 写入 32 位数据
// 调用前必须 flash_unlock;写完不需要单独 lock
bool flash_write_word(uint32_t addr, uint32_t data);

// 通用块写:将 buf[0..len) 写入内部 Flash 起始地址 addr
// len 必须为 4 的倍数;地址最好 4 字节对齐
// 调用者负责先擦除对应区域
bool flash_write(uint32_t addr, const uint8_t *buf, uint32_t len);

// 读校验:比较 Flash[addr..addr+len) 与 buf 是否一致
bool flash_verify(uint32_t addr, const uint8_t *buf, uint32_t len);

#endif
