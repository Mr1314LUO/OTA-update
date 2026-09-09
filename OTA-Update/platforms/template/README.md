# 新芯片平台移植模板

把本目录复制为 `platforms/<你的芯片名>/`，然后按本文件逐步填空。
**lib/ 与 os/ 目录不需要任何修改** —— 平台只需提供本目录列出的接口实现。

参考实现：`platforms/stm32f103/`（STM32F103C8T6，已跑通完整 OTA 流程）。

<br />

```Markdown
platforms/template/
├── README.md                    # 移植指南：接口契约总表 + 7 步移植流程
├── printf_lite.h / .c           # 轻量 printf（芯片无关，仅 putc 挂钩板级 UART）
├── drivers/
│   ├── board_uart.h             # 【契约】串口：init/putc/getc_nonblock/getc_timeout
│   ├── board_storage.h          # 【契约】外部存储：read/write/erase_range
│   ├── board_flash.h            # 【契约】内部 Flash：unlock/erase/write_word（Bootloader 用）
│   ├── hal_ota_impl.c           # hal_ota_instance 实现（system_reset 需填）
│   ├── firmware_source_impl.c  # g_spiflash_source 固件源（已完整，接 board_storage）
│   └── lzma_alloc.h / .c       # LZMA 静态分配器（芯片无关，基本可直接用）
├── bootloader/
│   ├── boot_protocol.h / .c     # upgrade_flag 读写（改分区地址宏即可）
│   ├── main.c                   # Bootloader 骨架：查标志→解压写 Flash→跳转
│   └── Makefile
└── app/
    ├── FreeRTOSConfig.h         # RTOS 配置（改主频/优先级位数/heap）
    ├── main.c                   # App 骨架：硬件初始化→ota_app_start()→UART 命令
    └── Makefile
```

<br />

***

## 一、平台必须提供的 5 个接口契约

lib/ 中的平台无关代码通过以下 5 个符号/头文件与平台对接：

| # | 接口                                    | 头文件（lib 提供声明）                                             | 平台需提供                                                              | 用在哪                                          |
| - | ------------------------------------- | --------------------------------------------------------- | ------------------------------------------------------------------ | -------------------------------------------- |
| 1 | `hal_ota_t hal_ota_instance`          | `lib/hal/hal_ota.h`                                       | [drivers/hal\_ota\_impl.c](drivers/hal_ota_impl.c)                 | App：FSM 调 `system_reset()` 触发软复位进 Bootloader |
| 2 | `firmware_source_t g_spiflash_source` | `lib/hal/firmware_source.h`                               | [drivers/firmware\_source\_impl.c](drivers/firmware_source_impl.c) | App + Bootloader：从外部存储读固件                    |
| 3 | `ISzAlloc g_McuAlloc`                 | `lib/lzma/sdk/7zTypes.h`                                  | [drivers/lzma\_alloc.c](drivers/lzma_alloc.c)                      | App + Bootloader：LZMA 静态内存分配器                |
| 4 | `boot_read/write_upgrade_flag()`      | [bootloader/boot\_protocol.h](bootloader/boot_protocol.h) | [bootloader/boot\_protocol.c](bootloader/boot_protocol.c)          | App 写 PENDING / Bootloader 读 PENDING 写 DONE  |
| 5 | `printf_lite()` + `printf` 宏          | `lib/app/printf.h`（!HOST\_SIM 时拉入）                        | [printf\_lite.c](printf_lite.c) / [printf\_lite.h](printf_lite.h)  | 所有日志输出重定向到芯片 UART                            |

> **符号名必须为** **`g_spiflash_source`** 与 `hal_ota_instance`：
> lib/app/ota\_main.c 的 MCU 路径（`ota_app_start()`）直接引用这两个符号，改名会导致链接失败。

## 二、平台还需提供的底层驱动（boot\_protocol / 固件下载 / Bootloader 依赖）

模板用 3 个头文件定义驱动契约，你在 `.c` 中按芯片手册实现寄存器操作：

| 契约头                                                 | 需实现函数                                             | 用途                                |
| --------------------------------------------------- | ------------------------------------------------- | --------------------------------- |
| [drivers/board\_uart.h](drivers/board_uart.h)       | `board_uart_init/putc/getc_nonblock/getc_timeout` | 日志 + App 端 UART 固件下载              |
| [drivers/board\_storage.h](drivers/board_storage.h) | `board_storage_init/read/write/erase_range`       | 外部存储（SPI Flash 等）：固件区 + 标志区       |
| [drivers/board\_flash.h](drivers/board_flash.h)     | `board_flash_unlock/lock/erase_range/write_word`  | 芯片**内部** Flash 擦写（仅 Bootloader 用） |

## 三、移植步骤

1. **复制模板**：`cp -r platforms/template platforms/<chip>`
2. **FreeRTOS 移植层**（仅 App 需要）：
   - 从 FreeRTOS 官方源码取对应架构的 port（如 `portable/GCC/ARM_CM3/port.c` + `portmacro.h`），
     放到 `platforms/<chip>/freertos_port/`；RISC-V 等其他架构同理。
   - 按芯片改 [app/FreeRTOSConfig.h](app/FreeRTOSConfig.h)：`configCPU_CLOCK_HZ`、`configPRIO_BITS`（优先级位数）、`configTOTAL_HEAP_SIZE`。
3. **启动文件**：提供芯片的 startup 汇编（向量表）+ 系统时钟初始化（如 `system_<chip>.c`）+ 链接脚本。
   链接脚本需支持 Bootloader/App 两分区（参考 stm32 的 `STM32F103C8Tx_FLASH.ld`：
   Bootloader 用 `--defsym FLASH_ORIGIN/FLASH_LENGTH` 覆盖起始地址与长度）。
4. **填驱动**：实现 board\_uart / board\_storage / board\_flash 三组函数（搜索文件内 `TODO`）。
5. **填 HAL 契约**：hal\_ota\_impl.c（系统复位）、firmware\_source\_impl.c（固件源）、boot\_protocol.c（标志地址）。
6. **入口**：[app/main.c](app/main.c) 初始化硬件后调 `ota_app_start()`；
   [bootloader/main.c](bootloader/main.c) 检查标志→解压写 Flash→跳转 App（跳转代码按架构调整 MSP/VTOR）。
7. **改 Makefile**：[app/Makefile](app/Makefile) 与 [bootloader/Makefile](bootloader/Makefile)
   中的 `CC`（交叉工具链）、`CPUFLAGS`（-mcpu/-mthumb 等）、源文件列表。

## 四、分区布局约定（在 boot\_protocol.h 配置）

```
内部 Flash:  [ Bootloader ][           App 区           ]
             <-- BOOT_SIZE  BOOT_BASE_ADDR
                          APP_BASE_ADDR ... APP_END_ADDR

外部存储:    [ 固件镜像区(FirmwareHeader_t + LZMA 数据) ][ 标志扇区 ]
              0 ............................  STORAGE_FLAG_OFFSET
```

- App 下载完固件 → `boot_write_upgrade_flag(UPGRADE_FLAG_PENDING)` → 软复位
- Bootloader 见 PENDING → 擦 App 区 → 流式解压写入 → 写 DONE → 跳 App
- App 启动见 DONE → 清标志、跳过本次自动检查（防重复升级）
- 掉电安全：PENDING 在升级成功前不清除，复位后 Bootloader 重试

## 五、构建验证

```bash
# App 固件
cd platforms/<chip>/app && make
# Bootloader 固件
cd platforms/<chip>/bootloader && make
```

对照 `platforms/stm32f103/` 的 size 输出检查 Flash/RAM 占用。
