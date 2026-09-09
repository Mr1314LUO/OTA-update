// boot_protocol.h —— App 与 Bootloader 共享的 OTA 升级协议（模板）
//
// 约定内部 Flash 分区 + 外部存储布局 + upgrade_flag 标志位。
// App 下载完固件后写 PENDING 并复位；Bootloader 见 PENDING 执行升级，完成写 DONE。
//
// TODO: 按芯片 Flash 大小/分区修改下列地址宏。
// 参考：platforms/stm32f103/bootloader/boot_protocol.h
#ifndef BOOT_PROTOCOL_H
#define BOOT_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

// === 内部 Flash 分区地址（TODO: 按芯片/链接脚本修改，必须与 .ld 一致） ===
#define BOOT_BASE_ADDR    0x08000000UL  // Bootloader 起始（内部 Flash 基址）
#define APP_BASE_ADDR     0x08004000UL  // App 起始（Bootloader 之后）
#define APP_MAX_SIZE      0x0000C000UL  // App 区大小
#define APP_END_ADDR      (APP_BASE_ADDR + APP_MAX_SIZE)

// === 外部存储布局 ===
// 固件镜像区（从偏移 0 开始，与 firmware_source_impl.c 一致）
#define STORAGE_FW_OFFSET     0x000000UL

// 标志区：外部存储末尾 4KB 扇区（TODO: 按介质容量修改，须扇区对齐）
#ifndef STORAGE_FLAG_OFFSET
#define STORAGE_FLAG_OFFSET   0x0FF000UL
#endif

// === upgrade_flag 标志值（4 字节 magic，小端存放） ===
#define UPGRADE_FLAG_NONE     0x00000000UL  // 空闲
#define UPGRADE_FLAG_PENDING  0x4F544150UL  // "OTAP" — 待升级
#define UPGRADE_FLAG_DONE     0x4F544144UL  // "OTAD" — 升级完成
#define UPGRADE_FLAG_ERROR    0x4F544145UL  // "OTAE" — 升级失败

// 读取 upgrade_flag（返回 magic 值，擦除态 0xFFFFFFFF 视为无效）
uint32_t boot_read_upgrade_flag(void);

// 写 upgrade_flag（内部先擦标志扇区再写），成功返回 true
bool boot_write_upgrade_flag(uint32_t magic);

#endif // BOOT_PROTOCOL_H
