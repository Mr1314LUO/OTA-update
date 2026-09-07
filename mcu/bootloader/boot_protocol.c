// boot_protocol.c —— upgrade_flag 读写实现(W25Q SPI Flash 元数据区)
#include "boot_protocol.h"
#include "w25qxx.h"

uint32_t boot_read_upgrade_flag(void) {
    uint8_t buf[4];
    w25qxx_read(SPI_FLASH_FLAG_OFFSET, buf, sizeof(buf));
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

bool boot_write_upgrade_flag(uint32_t magic) {
    uint8_t buf[4];
    buf[0] = (uint8_t)(magic & 0xFF);
    buf[1] = (uint8_t)((magic >> 8)  & 0xFF);
    buf[2] = (uint8_t)((magic >> 16) & 0xFF);
    buf[3] = (uint8_t)((magic >> 24) & 0xFF);
    // 元数据区在 SPI Flash 末尾 4KB 扇区,w25qxx_write_with_erase 会先擦后写
    return w25qxx_write_with_erase(SPI_FLASH_FLAG_OFFSET, buf, sizeof(buf));
}
