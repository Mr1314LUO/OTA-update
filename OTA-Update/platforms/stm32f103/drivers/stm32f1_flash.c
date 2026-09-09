// stm32f1_flash.c —— STM32F1 内部 Flash 擦写驱动(裸寄存器)
// 用于 Bootloader 擦写 App 区(0x08004000~0x0800FFFF)
//
// STM32F1 中容量 Flash 特性:
//   - 页大小 1KB(中容量),单 bank
//   - 字编程(32 位),半字编程(16 位)也可
//   - 解锁:KEYR 依次写入 KEY1/KEY2
//   - 擦页:CR |= PER,AR=addr,CR |= STRT
//   - 写字:CR |= PG,*(uint32_t*)addr = data,等 EOP
#include "stm32f1_flash.h"
#include "stm32f103xb.h"

// 简单 ms 延时(粗略,72MHz)
static void flash_delay_ms(uint32_t ms) {
    volatile uint32_t i;
    while (ms--) {
        for (i = 0; i < 6000; i++) {
            __asm__ volatile("nop");
        }
    }
}

void flash_unlock(void) {
    // 若 LOCK 位为 0,说明已解锁,直接返回
    if ((FLASH->CR & FLASH_CR_LOCK) == 0) return;
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;
}

void flash_lock(void) {
    FLASH->CR |= FLASH_CR_LOCK;
}

bool flash_wait_idle(uint32_t timeout_ms) {
    while (timeout_ms--) {
        if ((FLASH->SR & FLASH_SR_BSY) == 0) {
            return true;
        }
        flash_delay_ms(1);
    }
    return false;
}

bool flash_erase_page(uint32_t addr) {
    // 页对齐到 1KB
    addr &= ~((uint32_t)(STM32F1_FLASH_PAGE_SIZE - 1));

    // 等空闲
    if (!flash_wait_idle(50)) return false;

    // 清错误标志(PGERR/WRPRTERR 由写错或写保护触发,EOP 清)
    FLASH->SR = FLASH_SR_PGERR | FLASH_SR_WRPRTERR | FLASH_SR_EOP;

    // 设置页擦除 + 地址 + 启动
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR  = addr;
    FLASH->CR |= FLASH_CR_STRT;

    // 等完成(页擦典型 30-40ms,给 100ms)
    bool ok = flash_wait_idle(100);

    // 清 PER
    FLASH->CR &= ~FLASH_CR_PER;

    // 检查错误
    if (FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) {
        ok = false;
    }
    return ok;
}

bool flash_erase_range(uint32_t start, uint32_t len) {
    // 起始页对齐
    uint32_t page_mask = STM32F1_FLASH_PAGE_SIZE - 1;
    uint32_t aligned_start = start & ~page_mask;
    uint32_t aligned_end   = (start + len + page_mask) & ~page_mask;
    uint32_t pages = (aligned_end - aligned_start) / STM32F1_FLASH_PAGE_SIZE;

    for (uint32_t i = 0; i < pages; i++) {
        if (!flash_erase_page(aligned_start + i * STM32F1_FLASH_PAGE_SIZE)) {
            return false;
        }
    }
    return true;
}

bool flash_write_word(uint32_t addr, uint32_t data) {
    if (!flash_wait_idle(10)) return false;
    FLASH->SR = FLASH_SR_PGERR | FLASH_SR_WRPRTERR | FLASH_SR_EOP;

    // 设置 PG,然后直接写内存映射地址
    FLASH->CR |= FLASH_CR_PG;
    *(volatile uint32_t *)addr = data;

    // 等待 EOP(编程完成,典型 30-40us,给 10ms)
    bool ok = flash_wait_idle(10);
    FLASH->CR &= ~FLASH_CR_PG;

    if (FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) {
        ok = false;
    }
    return ok;
}

bool flash_write(uint32_t addr, const uint8_t *buf, uint32_t len) {
    if (len == 0) return true;
    // 长度必须 4 字节倍数(简化;Bootloader 流式写会保证 4 字节对齐块)
    if ((len % 4) != 0) return false;

    const uint32_t *p = (const uint32_t *)buf;
    uint32_t words = len / 4;

    for (uint32_t i = 0; i < words; i++) {
        if (!flash_write_word(addr + i * 4, p[i])) {
            return false;
        }
    }
    return true;
}

bool flash_verify(uint32_t addr, const uint8_t *buf, uint32_t len) {
    const uint8_t *flash = (const uint8_t *)addr;
    for (uint32_t i = 0; i < len; i++) {
        if (flash[i] != buf[i]) {
            return false;
        }
    }
    return true;
}
