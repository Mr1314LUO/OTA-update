// hal_ota_impl.c —— 平台 hal_ota_t 实例（模板）
//
// 提供 lib/hal/hal_ota.h 声明、lib/app/ota_main.c 引用的全局符号 hal_ota_instance。
//
// App 固件职责：OTA 状态机只在升级末尾调用 system_reset() 触发软复位，
//   由 Bootloader 接管完成实际擦写；因此 flash_erase/write/read 在 App 中是桩。
//   （内部 Flash 的真正擦写由 Bootloader 用 board_flash.h 接口完成。）
//
// TODO: 实现 board_system_reset() —— 按架构写复位寄存器：
//   Cortex-M : 关中断 + SCB->AIRCR = (0x5FA<<16) | (1<<2)（SYSRESETREQ）
//   RISC-V   : 写看门狗复位 / MSIP 软复位寄存器
//   其他     : 查阅芯片手册的 software reset 章节
//
// 参考：platforms/stm32f103/drivers/stm32f1_hal_ota.c
#include "hal/hal_ota.h"

// TODO: 替换为芯片头文件，如 #include "chip.h"
// #include "your_chip_header.h"

// 触发芯片软复位（不返回）
static void board_system_reset(void) {
    // TODO: 按芯片手册实现软复位。Cortex-M 示例：
    //   __asm volatile ("cpsid i" ::: "memory");
    //   SCB->AIRCR = (0x5FAu << 16) | (1u << 2);
    //   while (1) { }
    while (1) { }  // 占位：未实现时死循环，避免跑飞
}

// App 不直接擦写内部 Flash（Bootloader 负责），以下为桩
static bool board_flash_erase(uint32_t addr, uint32_t size,
                              erase_progress_cb_t cb, const char *name) {
    (void)addr; (void)size; (void)cb; (void)name;
    return false;
}
static bool board_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    (void)addr; (void)data; (void)len;
    return false;
}
static void board_flash_read(uint32_t addr, uint8_t *data, uint32_t len) {
    (void)addr; (void)data; (void)len;
}

// 全局实例 —— 符号名必须为 hal_ota_instance（lib/app/ota_main.c 直接引用）
hal_ota_t hal_ota_instance = {
    .flash_erase  = board_flash_erase,
    .flash_write  = board_flash_write,
    .flash_read   = board_flash_read,
    .system_reset = board_system_reset,
};
