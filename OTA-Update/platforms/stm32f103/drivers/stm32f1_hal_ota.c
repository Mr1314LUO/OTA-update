// stm32f1_hal_ota.c —— MCU hal_ota_t 实例
// P4 阶段:App 内 OTA 任务只需 system_reset() 触发软复位交由 Bootloader 接管;
//          flash_erase/write/read 由 Bootloader 在 P6 单独实现(此处桩实现)
// 链接时提供 ota_fsm.c / ota_main.c 引用的 hal_ota_instance 符号
#include "hal/hal_ota.h"
#include "stm32f103xb.h"

// 触发 Cortex-M3 软复位(NVIC AIRCR reset)
static void mcu_system_reset(void) {
    __asm volatile ("cpsid i" ::: "memory");
    SCB->AIRCR = (0x5FA << 16) | (1u << 2);  // VECTKEY=0x5FA, SYSRESETREQ=1
    while (1) { }
}

// P4 桩:App 内 OTA 不直接擦写内部 Flash,实际擦写由 Bootloader 接管
static bool mcu_flash_erase(uint32_t addr, uint32_t size,
                             erase_progress_cb_t cb, const char *name) {
    (void)addr; (void)size; (void)cb; (void)name;
    return false;
}
static bool mcu_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    (void)addr; (void)data; (void)len;
    return false;
}
static void mcu_flash_read(uint32_t addr, uint8_t *data, uint32_t len) {
    (void)addr; (void)data; (void)len;
}

hal_ota_t hal_ota_instance = {
    .flash_erase  = mcu_flash_erase,
    .flash_write  = mcu_flash_write,
    .flash_read   = mcu_flash_read,
    .system_reset = mcu_system_reset,
};
