// lzma_alloc.h —— LZMA 解码器静态内存分配器（MCU 用，模板）
//
// 替代 lib/lzma/sdk/Alloc.c（依赖 POSIX malloc/unistd/sys-mman，无法上 MCU）。
// 静态 arena + bump 分配，Free 不回收（单次解码，够用）。
//
// 提供 lib/lzma/flow-unzip/unzip_stream.h 在 !HOST_SIM 路径引用的 g_McuAlloc
// （经 g_FirmwareAlloc 宏使用）。
// 参考：platforms/stm32f103/drivers/lzma_alloc.h（本文件基本可直接复制使用）
#ifndef LZMA_ALLOC_H
#define LZMA_ALLOC_H

#include "7zTypes.h"

// 静态 arena 大小：lc=0/lp=0/pb=0/dict=4KB 时 LZMA probs ~5.4KB，给 6KB 留余量。
// 若固件压缩参数不同（更大字典），需相应增大。
#define LZMA_ARENA_SIZE  (6 * 1024)

// 全局静态分配器实例（替代 g_Alloc）
extern const ISzAlloc g_McuAlloc;

#endif // LZMA_ALLOC_H
