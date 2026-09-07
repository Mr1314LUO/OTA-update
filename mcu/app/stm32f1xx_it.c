// stm32f1xx_it.c —— 中断处理(P0 仅留空,FreeRTOS 阶段会改写 SysTick/PendSV/SVC)
// 启动文件已用 .weak 默认指向 Default_Handler,这里仅提供兜底
#include "stm32f103xb.h"

// 默认中断处理(覆盖启动文件 Default_Handler)
void Default_Handler(void) {
    for (;;) { }
}
