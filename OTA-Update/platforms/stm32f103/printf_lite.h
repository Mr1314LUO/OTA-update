// printf_lite.h —— 轻量 printf/snprintf,直接调 uart1_putc,无 _sbrk/malloc
// 支持 %d %i %u %x %X %s %c %p %%,长度修饰 l/ll/z,0 填充宽度
//
// 在 MCU 上通过 #define 把 printf / snprintf 重定向到 printf_lite / snprintf_lite,
// 使 printf.h 的 LOG_* 宏与 ota_fsm.c 等处 snprintf 调用均不拉入 newlib 的
// vfprintf/sscanf/dtoa(默认 ~20KB,装不下 48KB App 区)。
//
// 注意:本头必须先于 printf.h 被 include(或由 printf.h 在 !HOST_SIM 时拉入),
//       否则 printf.h 中的 LOG_* 宏展开仍引用 newlib printf。
#ifndef PRINTF_LITE_H
#define PRINTF_LITE_H

#include <stdint.h>
#include <stddef.h>

int printf_lite(const char *fmt, ...);
int snprintf_lite(char *buf, size_t size, const char *fmt, ...);

// 把 printf.h 中的 printf 调用重定向到 printf_lite
#define printf printf_lite
// 把 ota_fsm.c 等处的 snprintf 调用重定向到 snprintf_lite(避免拉入 newlib vfprintf)
#define snprintf snprintf_lite

#endif // PRINTF_LITE_H
