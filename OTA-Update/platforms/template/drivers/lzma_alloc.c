// lzma_alloc.c —— LZMA 解码器静态内存分配器（MCU 用，模板）
//
// 设计：静态 arena + bump 分配
//   - Alloc：从 bump 指针线性增长，返回 4 字节对齐地址
//   - Free ：无操作（单次解码，不复用）
// 本文件与芯片无关，通常可直接编译，仅需确认 arena 大小与 CRC32 stub（见末尾）。
// 参考：platforms/stm32f103/drivers/lzma_alloc.c
#include "lzma_alloc.h"
#include <stdint.h>
#include <stddef.h>

static uint8_t g_lzma_arena[LZMA_ARENA_SIZE];
static size_t  g_lzma_offset = 0;

static void *mcu_alloc(ISzAllocPtr p, size_t size) {
    (void)p;
    if (size == 0) return NULL;

    // 4 字节对齐
    size_t aligned = (size + 3u) & ~((size_t)3);

    if (g_lzma_offset + aligned > LZMA_ARENA_SIZE) {
        return NULL;  // arena 不足，增大 LZMA_ARENA_SIZE
    }

    void *ptr = &g_lzma_arena[g_lzma_offset];
    g_lzma_offset += aligned;
    return ptr;
}

static void mcu_free(ISzAllocPtr p, void *address) {
    (void)p;
    (void)address;  // 无操作：静态 arena，不回收
}

const ISzAlloc g_McuAlloc = { mcu_alloc, mcu_free };

/* ================================================================
 * CPU_IsSupported_CRC32 stub
 *
 * LZMA SDK 7zCrc.c 在 ARM GCC（__GNUC__>=8 + Cortex-M）路径下会误判支持
 * ARMv8 CRC32 硬件指令（Cortex-M0/M3 等不支持），从而引用
 * CPU_IsSupported_CRC32（声明在 CpuArch.h，SDK 未提供 CpuArch.c 实现）。
 * 这里提供 stub 返回 0（不支持），强制走软件表 CRC 路径。
 *
 * 若你的芯片确实支持 ARMv8 CRC 指令（Cortex-M33/M55 等），可返回 1 走硬件加速；
 * 非 ARM 架构（RISC-V 等）7zCrc.c 不引用此符号，本 stub 会成为未使用函数
 * （不影响链接，必要时可加 __attribute__((unused))）。
 * ================================================================ */
#ifndef HOST_SIM
BoolInt CPU_IsSupported_CRC32(void) { return 0; }
#endif
