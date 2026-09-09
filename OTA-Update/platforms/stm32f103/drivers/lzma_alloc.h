// lzma_alloc.h —— LZMA 解码器静态内存分配器(MCU 用)
// 替代 Alloc.c(依赖 POSIX malloc/unistd/sys-mman,无法上 MCU)
// 使用静态 arena + bump 分配,Free 不回收内存(单次解码,够用)
#ifndef LZMA_ALLOC_H
#define LZMA_ALLOC_H

#include "7zTypes.h"

// 静态 arena 大小:lc=0/lp=0/pb=0/dict=4KB 时 LZMA probs ~5.4KB
// 给 6KB 留余量
#define LZMA_ARENA_SIZE  (6 * 1024)

// 全局静态分配器实例(替代 g_Alloc)
extern const ISzAlloc g_McuAlloc;

#endif
