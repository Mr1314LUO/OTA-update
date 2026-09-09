// board_flash.h —— 芯片内部 Flash 擦写驱动契约（模板，仅 Bootloader 使用）
//
// Bootloader 把解压后的固件写入芯片内部 Flash 的 App 分区，需要本接口。
// App 固件本身不擦写内部 Flash（hal_ota_impl.c 中相关函数为桩）。
// 参考：platforms/stm32f103/drivers/stm32f1_flash.c
//
// 注意：不同芯片 Flash 编程位宽不同（1/2/4 字节），按手册调整 write_word 的实现。
//       若最小编程单位不是 4 字节，需同步修改 bootloader/main.c 的 tail 缓冲逻辑。
#ifndef BOARD_FLASH_H
#define BOARD_FLASH_H

#include <stdint.h>
#include <stdbool.h>

// 内部 Flash 页/扇区大小（按芯片手册修改；STM32F1 中容量为 1KB）
#define BOARD_FLASH_PAGE_SIZE  0x400

// 解锁 Flash 控制器（写解锁序列）
void board_flash_unlock(void);

// 锁定 Flash 控制器
void board_flash_lock(void);

// 擦除覆盖 [start, start+len) 的所有页（start 自动页对齐，len 向上取整）
// 成功返回 true
bool board_flash_erase_range(uint32_t start, uint32_t len);

// 字编程：向 4 字节对齐地址写入 32 位数据（调用前需 unlock + 已擦除）
// 成功返回 true
bool board_flash_write_word(uint32_t addr, uint32_t data);

#endif // BOARD_FLASH_H
