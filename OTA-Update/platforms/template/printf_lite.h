// printf_lite.h —— 轻量 printf/snprintf 重定向（模板，芯片无关）
//
// 通过 #define 把 printf / snprintf 重定向到 printf_lite / snprintf_lite，
// 使 lib/app/printf.h 的 LOG_* 宏与 ota_fsm.c 等处的 snprintf 调用都走
// 本实现（不依赖 newlib 的 vfprintf/dtoa，省约 20KB Flash）。
//
// 输出后端由 printf_lite.c 调用 board_uart_putc() 完成（见 board_uart.h）。
//
// 注意：本头必须先于 printf.h 被 include（lib/app/printf.h 在 !HOST_SIM 时
//       会自动 #include "printf_lite.h"，正常使用无需手动处理）。
// 参考：platforms/stm32f103/printf_lite.h（本文件可直接复制使用）
#ifndef PRINTF_LITE_H
#define PRINTF_LITE_H

#include <stdint.h>
#include <stddef.h>

int printf_lite(const char *fmt, ...);
int snprintf_lite(char *buf, size_t size, const char *fmt, ...);

// 重定向：lib 代码中的 printf/snprintf 调用改走轻量实现
#define printf   printf_lite
#define snprintf snprintf_lite

#endif // PRINTF_LITE_H
