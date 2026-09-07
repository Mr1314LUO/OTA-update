// lzma_alloc.c —— LZMA 解码器静态内存分配器(MCU 用)
// 替代 Alloc.c(依赖 POSIX malloc/unistd/sys-mman,无法上 MCU)
//
// 设计:静态 arena + bump 分配
//   - Alloc:从 bump 指针线性增长,返回 4 字节对齐地址
//   - Free:无操作(单次解码,不复用)
//   - arena 在 .bss,20KB RAM 内空间充足
//
// 容量验证:lc=0/lp=0/pb=0/dict=4KB → LZMA probs ~5.4KB,6KB arena 足够
#include "lzma_alloc.h"
#include <stdint.h>
#include <stddef.h>

static uint8_t g_lzma_arena[LZMA_ARENA_SIZE];
static size_t g_lzma_offset = 0;

static void *mcu_alloc(ISzAllocPtr p, size_t size) {
    (void)p;
    if (size == 0) return NULL;

    // 4 字节对齐
    size_t aligned = (size + 3u) & ~((size_t)3);

    // 容量检查
    if (g_lzma_offset + aligned > LZMA_ARENA_SIZE) {
        return NULL;  // arena 不足
    }

    void *ptr = &g_lzma_arena[g_lzma_offset];
    g_lzma_offset += aligned;
    return ptr;
}

static void mcu_free(ISzAllocPtr p, void *address) {
    (void)p;
    (void)address;  // 无操作:静态 arena,不回收
}

const ISzAlloc g_McuAlloc = { mcu_alloc, mcu_free };

/* ================================================================
 * CPU_IsSupported_CRC32 stub
 *
 * LZMA SDK 7zCrc.c 在 ARM 路径下(__GNUC__>=8 + Cortex-M)会误判
 * 支持 ARMv8 CRC32 硬件指令(实际 Cortex-M3 不支持),引用
 * CPU_IsSupported_CRC32(声明在 CpuArch.h,实现本应在 CpuArch.c,
 * 但 SDK 未提供 CpuArch.c)。此处提供 stub 返回 0(不支持),
 * 强制走软件表 CRC 路径。
 *
 * 注:仅 MCU 路径需要(HOST_SIM 走 x86 路径不引用此符号)。
 * ================================================================ */
#ifndef HOST_SIM
/* BoolInt 已由 7zTypes.h 定义(lzma_alloc.h → 7zTypes.h) */
BoolInt CPU_IsSupported_CRC32(void) { return 0; }
#endif
