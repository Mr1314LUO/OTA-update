# FreeRTOS 移植变更摘要

**项目**：Bootloader-ota（OTA 固件升级框架）
**移植方式**：FreeRTOS-Kernel 官方 POSIX 移植层（Linux 主机模拟）
**内核版本**：FreeRTOS-Kernel V11（git `8be86d4`，MIT License）
**验证结果**：升级链路 `IDLE → CHECKING → DOWNLOADING → VERIFYING → READY → UPDATING → SUCCESS`，分区升级 Summary `Success: 5 / Failed: 0`

## 1. 变更文件总览

| 类型 | 文件 | 说明 |
|------|------|------|
| 新增 | `OTA-Update/module/freertos/` | FreeRTOS 内核子集（list/queue/tasks/timers/event_groups/heap_4） |
| 新增 | `freertos/posix/port.c`、`portmacro.h` | POSIX 移植层（任务=pthread，tick=SIGALRM，切换=SIGUSR1） |
| 新增 | `freertos/posix/utils/wait_for_event.c` | 移植层依赖的事件工具 |
| 新增 | `freertos/FreeRTOSConfig.h` | 内核配置（tick 1kHz、堆 1MB、优先级 8 级、启用定时器） |
| 新增 | `freertos/LICENSE.md` | 内核 MIT 许可证 |
| 重写 | `OTA-Update/app/ota_main.c` | `while(1)+usleep` 裸循环 → FreeRTOS 三任务 + 队列架构 |
| 修改 | `OTA-Update/module/fsm/ota_fsm.h` | 新增平台异步钩子声明（`ota_platform_*`） |
| 修改 | `OTA-Update/module/fsm/ota_fsm.c` | 下载/确认动作由内联同步改为异步钩子，状态停留等待事件回送 |
| 修改 | `OTA-Update/hal/hal_ota.c` | `usleep` → `host_delay_us()`：FreeRTOS 下走 `vTaskDelay` |
| 修改 | `OTA-Update/printf.h` | 所有打印用互斥锁串行化（POSIX 移植层强制要求） |
| 修改 | `Makefile`（根） | 加入内核源文件、include 路径、`-pthread`、`-DUSE_FREERTOS` |
| 修改 | `OTA-Update/Makefile` | `ota`/`run` 目标委托根 Makefile；模块工具 bin 保持无 RTOS 独立构建 |

## 2. 任务架构

```
main() ── 创建队列/互斥锁/任务 ──► vTaskStartScheduler()

  download_task ──EVENT_DOWNLOAD_COMPLETE──►┐
                                            ▼
  confirm_task  ──EVENT_READY_CONFIRM──► ota_task（状态机所有者，优先级最高）
                                            │ fsm_handle_event()
                                   动作中通过平台钩子反向发起请求：
                                   ota_platform_download_request() → download 队列
                                   ota_platform_request_confirm()   → 任务通知
```

| 任务 | 优先级 | 职责 | 阻塞机制 |
|------|--------|------|----------|
| `ota_task` | 3 | 拥有状态机上下文，从事件队列取事件驱动转移 | `xQueueReceive(xEventQueue, portMAX_DELAY)` |
| `download_task` | 2 | 模拟分块下载（4KB/块，vTaskDelay 2ms），更新进度 | `xQueueReceive(xDownloadQueue)` |
| `confirm_task` | 2 | 模拟用户确认（延时 1s 后确认） | `ulTaskNotifyTake` |

> 说明：POSIX 移植层任务实际运行在独立 pthread 中，栈深度参数仅用于容纳移植层线程控制结构（`configMINIMAL_STACK_SIZE` 仅需保证大于 `Thread_t`，FreeRTOSConfig.h 注释已注明）。另由内核自动创建 IDLE 任务与 Timer 服务任务（`configUSE_TIMERS=1`，port.c 引用 timers.h）。

## 3. 同步/通信对象

| 对象 | 类型 | 方向 | 用途 |
|------|------|------|------|
| `xEventQueue` | Queue(8 × ota_event_t) | download/confirm → ota | 事件驱动状态机 |
| `xDownloadQueue` | Queue(2 × uint32_t) | ota → download | 下载请求（携带升级包大小） |
| 任务通知 | Task Notification | ota → confirm | 触发用户确认流程 |
| `g_stdio_mutex` | Mutex | 全局 | printf 串行化，调度器启动前创建 |

## 4. 关键移植适配点

1. **FSM 异步化**：`action_start_download` / `action_prepare_update` 原本在动作内联完成下载与自动确认；现改为调用平台钩子后返回，状态机停留在 `DOWNLOADING`/`READY`，由其他任务回送 `EVENT_DOWNLOAD_COMPLETE`/`EVENT_READY_CONFIRM` —— 这两个钩子即真实设备对接 HTTP/MQTT 网络任务与用户 UI 的接入点。
2. **stdio 死锁规避**：POSIX 移植层官方注释明确要求 printf 必须跨任务串行化（任务被挂起时若持有 libc 内部锁，其他任务永久阻塞）。`printf.h` 中 `LOG_*`/`LOG_RAW`/`log_hex_dump` 全部由 `g_stdio_mutex` 包裹；非 RTOS 构建（`USE_FREERTOS` 未定义）时空实现，模块工具 bin 行为不变。
3. **延时替换**：任务上下文中 `usleep` 会阻塞 pthread 且可能被 tick 信号（SIGALRM）干扰；HAL 桩延时统一为 `host_delay_us()`（微秒向上取整到 tick，最短 1 tick）。
4. **钩子实现**：`vAssertCalled` / `vApplicationMallocFailedHook` 实现在 `ota_main.c`（fprintf 到 stderr 后 abort）。

## 5. 构建方式

```bash
make            # 构建并运行 FreeRTOS 版主应用（build/ota_main）
make run        # 同上
make clean
```

- 根 Makefile 新增：`-pthread`（编译与链接）、`-DUSE_FREERTOS`、freertos 三个 include 路径及 8 个内核/移植层源文件。
- 模块独立工具（firmware_create / flow-unzip / unzip / md5sum）在 `OTA-Update/Makefile` 中不带 `USE_FREERTOS` 编译，与内核完全解耦；其 `ota`/`run` 目标通过 `make -C ..` 委托根 Makefile。

## 6. 行为与验证注意事项

- 到达 `SUCCESS`/`FAILED` 终态后任务驻留（与原裸循环设计一致），Ctrl+C 退出；自动化验证用 `timeout`。
- 管道重定向时 stdout 为块缓冲，验证运行建议加 `stdbuf -o0`（LOG_RAW 自带 fflush，故进度行可见、最终日志易被误判丢失）。
- 已通过：状态转移日志 7 条完整、分区升级 5 成功 0 失败、`升级成功，固件版本已更新`。

## 7. 后续上真实 MCU 的迁移要点

- 替换 `freertos/posix` 为对应架构移植层（如 `portable/GCC/ARM_CM3`），`configMINIMAL_STACK_SIZE` 此时变为真实栈深，需按 4KB 解压缓冲所在任务重新核算。
- `g_stdio_mutex` 可保留（串口输出同样需要互斥），或替换为串口 DMA 发送。
- `host_delay_us` 中的 `vTaskDelay` 替换为真实 HAL 擦写时序（擦写期间建议挂起调度器或使用独立任务）。
- 平台钩子（`ota_platform_download_request` / `ota_platform_request_confirm`）对接真实网络栈与按键/UI 事件，状态机本身无需改动。
