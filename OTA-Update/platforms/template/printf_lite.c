// printf_lite.c —— 轻量 printf/snprintf 实现（模板）
//
// 格式化内核（vformat）与芯片无关；唯一平台依赖是 put_uart() 里的
// board_uart_putc()（见 drivers/board_uart.h），由你实现。
// 支持 %d %i %u %x %X %s %c %p %%，长度修饰 l/ll/z，0 填充宽度。
// 参考：platforms/stm32f103/printf_lite.c
#include "printf_lite.h"
#include "board_uart.h"   // TODO: 提供 board_uart_putc()
#include <stdarg.h>

// ==========================================
// 输出适配器：统一 put 接口，目标可为 UART 或缓冲区
// ==========================================
typedef struct {
    void (*put)(void *ctx, char c);
    void *ctx;
} lite_out_t;

// UART 输出：忽略 ctx，直接送板级 putc
static void put_uart(void *ctx, char c) {
    (void)ctx;
    board_uart_putc(c);
}

// 缓冲区输出：写入 buf，超长截断（pos+1<size 保证末尾 NUL）
typedef struct {
    char  *buf;
    size_t size;
    size_t pos;
} lite_buf_t;

static void put_buf(void *ctx, char c) {
    lite_buf_t *b = (lite_buf_t *)ctx;
    if (b->pos + 1 < b->size) {
        b->buf[b->pos++] = c;
    }
}

// ==========================================
// 数值格式化
// ==========================================
static void emit_uint(lite_out_t *o, uint32_t val, int base, int upper,
                      int width, char pad) {
    char buf[10];
    const char *d = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;
    if (val == 0) {
        buf[i++] = '0';
    }
    while (val > 0 && i < (int)sizeof(buf)) {
        buf[i++] = d[val % (uint32_t)base];
        val /= (uint32_t)base;
    }
    for (int j = i; j < width; j++) {
        o->put(o->ctx, pad);
    }
    while (i > 0) {
        o->put(o->ctx, buf[--i]);
    }
}

static void emit_int(lite_out_t *o, int32_t val, int width, char pad) {
    if (val < 0) {
        o->put(o->ctx, '-');
        emit_uint(o, (uint32_t)(-val), 10, 0, width > 0 ? width - 1 : 0, pad);
    } else {
        emit_uint(o, (uint32_t)val, 10, 0, width, pad);
    }
}

static void emit_str(lite_out_t *o, const char *s) {
    if (!s) {
        s = "(null)";
    }
    while (*s) {
        o->put(o->ctx, *s++);
    }
}

// ==========================================
// 共享格式化内核
// ==========================================
static void vformat(lite_out_t *o, const char *fmt, va_list ap) {
    while (*fmt) {
        if (*fmt != '%') {
            o->put(o->ctx, *fmt++);
            continue;
        }
        fmt++; // 跳过 '%'
        char pad = ' ';
        if (*fmt == '0') { pad = '0'; fmt++; }
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        int is_ll = 0;
        if (*fmt == 'l') {
            fmt++;
            if (*fmt == 'l') { is_ll = 1; fmt++; }
        } else if (*fmt == 'z') {
            fmt++; // 32 位 MCU 上 size_t = unsigned int
        }
        switch (*fmt) {
        case 'd': case 'i': {
            int32_t v = is_ll ? (int32_t)va_arg(ap, long long) : va_arg(ap, int);
            emit_int(o, v, width, pad);
            break;
        }
        case 'u': {
            uint32_t v = is_ll ? (uint32_t)va_arg(ap, unsigned long long)
                               : va_arg(ap, unsigned int);
            emit_uint(o, v, 10, 0, width, pad);
            break;
        }
        case 'x': {
            uint32_t v = is_ll ? (uint32_t)va_arg(ap, unsigned long long)
                               : va_arg(ap, unsigned int);
            emit_uint(o, v, 16, 0, width, pad);
            break;
        }
        case 'X': {
            uint32_t v = is_ll ? (uint32_t)va_arg(ap, unsigned long long)
                               : va_arg(ap, unsigned int);
            emit_uint(o, v, 16, 1, width, pad);
            break;
        }
        case 's': {
            emit_str(o, va_arg(ap, const char *));
            break;
        }
        case 'c': {
            o->put(o->ctx, (char)va_arg(ap, int));
            break;
        }
        case 'p': {
            uint32_t v = (uint32_t)(uintptr_t)va_arg(ap, void *);
            emit_str(o, "0x");
            emit_uint(o, v, 16, 0, 8, '0');
            break;
        }
        case '%':
            o->put(o->ctx, '%');
            break;
        default:
            o->put(o->ctx, '%');
            if (*fmt) o->put(o->ctx, *fmt);
            break;
        }
        if (*fmt) fmt++;
    }
}

// ==========================================
// 公开接口
// ==========================================
int printf_lite(const char *fmt, ...) {
    lite_out_t o = { put_uart, NULL };
    va_list ap;
    va_start(ap, fmt);
    vformat(&o, fmt, ap);
    va_end(ap);
    return 0;
}

int snprintf_lite(char *buf, size_t size, const char *fmt, ...) {
    if (buf == NULL || size == 0) {
        return 0;
    }
    lite_buf_t b = { buf, size, 0 };
    lite_out_t o = { put_buf, &b };
    va_list ap;
    va_start(ap, fmt);
    vformat(&o, fmt, ap);
    va_end(ap);
    b.buf[b.pos] = '\0';
    return (int)b.pos;
}
