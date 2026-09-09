// boot_protocol.c —— upgrade_flag 读写实现（模板）
//
// 使用 board_storage 抽象后端（外部存储）存放 4 字节标志。
// 参考：platforms/stm32f103/bootloader/boot_protocol.c
#include "boot_protocol.h"
#include "board_storage.h"

uint32_t boot_read_upgrade_flag(void) {
    uint8_t buf[4];
    board_storage_read(STORAGE_FLAG_OFFSET, buf, sizeof(buf));
    return (uint32_t)buf[0]        | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

bool boot_write_upgrade_flag(uint32_t magic) {
    uint8_t buf[4];
    buf[0] = (uint8_t)(magic & 0xFF);
    buf[1] = (uint8_t)((magic >> 8)  & 0xFF);
    buf[2] = (uint8_t)((magic >> 16) & 0xFF);
    buf[3] = (uint8_t)((magic >> 24) & 0xFF);
    // 标志区在独立扇区，board_storage_write_with_erase 先擦后写
    return board_storage_write_with_erase(STORAGE_FLAG_OFFSET, buf, sizeof(buf));
}
