// w25qxx.c —— W25Qxx SPI Flash 驱动(读 + 写 + 擦,轮询)
#include "w25qxx.h"
#include "stm32f1_spi.h"

// JEDEC ID 命令
#define W25Q_CMD_JEDEC_ID   0x9F
// 读数据命令(支持任意地址,连续读)
#define W25Q_CMD_READ_DATA  0x03
// 读状态寄存器 1
#define W25Q_CMD_READ_SR    0x05
// 写使能
#define W25Q_CMD_WRITE_EN   0x06
// 页编程(最多 256B,且不跨 256B 页边界)
#define W25Q_CMD_PAGE_PROG 0x02
// 扇区擦除(4KB)
#define W25Q_CMD_SECT_ERASE 0x20

// 状态寄存器位
#define W25Q_SR_WIP         0x01  // Write In Progress(bsy)

// 简单延时(粗略):SPI 18MHz 下一次 transfer 约 0.5us
// 用空循环做 ms 级粗延时,避免依赖 SysTick
static void w25q_delay_ms(uint32_t ms) {
    // 约 72MHz 主频下,nop 循环近似 1ms(不精确,仅用于超时保护)
    volatile uint32_t i;
    while (ms--) {
        for (i = 0; i < 6000; i++) {
            __asm__ volatile("nop");
        }
    }
}

void w25qxx_read_id(uint8_t *manufacturer, uint8_t *memory_type, uint8_t *capacity) {
    spi1_cs_low();
    spi1_transfer(W25Q_CMD_JEDEC_ID);
    *manufacturer  = spi1_transfer(0x00);
    *memory_type   = spi1_transfer(0x00);
    *capacity      = spi1_transfer(0x00);
    spi1_cs_high();
}

void w25qxx_read(uint32_t addr, uint8_t *buf, uint32_t len) {
    spi1_cs_low();
    spi1_transfer(W25Q_CMD_READ_DATA);
    spi1_transfer((addr >> 16) & 0xFF);
    spi1_transfer((addr >> 8)  & 0xFF);
    spi1_transfer(addr & 0xFF);
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = spi1_transfer(0x00);
    }
    spi1_cs_high();
}

uint8_t w25qxx_read_status(void) {
    spi1_cs_low();
    spi1_transfer(W25Q_CMD_READ_SR);
    uint8_t sr = spi1_transfer(0x00);
    spi1_cs_high();
    return sr;
}

bool w25qxx_wait_idle(uint32_t timeout_ms) {
    while (timeout_ms--) {
        if ((w25qxx_read_status() & W25Q_SR_WIP) == 0) {
            return true;
        }
        w25q_delay_ms(1);
    }
    return false;  // 超时
}

void w25qxx_write_enable(void) {
    spi1_cs_low();
    spi1_transfer(W25Q_CMD_WRITE_EN);
    spi1_cs_high();
}

bool w25qxx_erase_sector(uint32_t addr) {
    // 扇区对齐到 4KB
    addr &= ~((uint32_t)(W25Q_SECTOR_SIZE - 1));

    w25qxx_write_enable();
    if (!w25qxx_wait_idle(50)) return false;

    spi1_cs_low();
    spi1_transfer(W25Q_CMD_SECT_ERASE);
    spi1_transfer((addr >> 16) & 0xFF);
    spi1_transfer((addr >> 8)  & 0xFF);
    spi1_transfer(addr & 0xFF);
    spi1_cs_high();

    // 扇区擦除典型 60-400us,最坏 3s(大容量),给 500ms 上限
    return w25qxx_wait_idle(500);
}

bool w25qxx_program_page(uint32_t addr, const uint8_t *data, uint32_t len) {
    // 检查不跨页边界
    uint32_t page_rem = W25Q_PAGE_SIZE - (addr % W25Q_PAGE_SIZE);
    if (len > page_rem) return false;  // 跨页,调用者应拆分

    w25qxx_write_enable();
    if (!w25qxx_wait_idle(50)) return false;

    spi1_cs_low();
    spi1_transfer(W25Q_CMD_PAGE_PROG);
    spi1_transfer((addr >> 16) & 0xFF);
    spi1_transfer((addr >> 8)  & 0xFF);
    spi1_transfer(addr & 0xFF);
    for (uint32_t i = 0; i < len; i++) {
        spi1_transfer(data[i]);
    }
    spi1_cs_high();

    // 页编程典型 0.7-1.5ms,给 50ms 上限
    return w25qxx_wait_idle(50);
}

bool w25qxx_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    while (len > 0) {
        uint32_t page_rem = W25Q_PAGE_SIZE - (addr % W25Q_PAGE_SIZE);
        uint32_t chunk = (len < page_rem) ? len : page_rem;
        if (!w25qxx_program_page(addr, data, chunk)) {
            return false;
        }
        addr  += chunk;
        data  += chunk;
        len   -= chunk;
    }
    return true;
}

bool w25qxx_write_with_erase(uint32_t addr, const uint8_t *data, uint32_t len) {
    // 仅支持单扇区内写(用于元数据/标志位回写)
    uint32_t sec_start = addr & ~((uint32_t)(W25Q_SECTOR_SIZE - 1));
    uint32_t sec_end   = sec_start + W25Q_SECTOR_SIZE;
    if ((addr + len) > sec_end) return false;

    if (!w25qxx_erase_sector(addr)) return false;
    return w25qxx_write(addr, data, len);
}

// P7: 擦除覆盖 [addr, addr+len) 的所有 4KB 扇区
bool w25qxx_erase_range(uint32_t addr, uint32_t len) {
    if (len == 0) return true;
    // 计算首尾扇区地址
    uint32_t sec = addr & ~((uint32_t)(W25Q_SECTOR_SIZE - 1));
    uint32_t end = addr + len;
    while (sec < end) {
        if (!w25qxx_erase_sector(sec)) return false;
        sec += W25Q_SECTOR_SIZE;
    }
    return true;
}
