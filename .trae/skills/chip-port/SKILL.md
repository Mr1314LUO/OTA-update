---
name: "chip-port"
description: "将 OTA 框架移植到新芯片平台的标准工作流。当用户要求迁移到新 MCU/芯片、新增 platform、适配新硬件、或询问如何移植到别的芯片时调用。基于 platforms/template 模板执行契约检查、驱动填空、构建验证。"
---

# 芯片平台移植工作流（chip-port）

将 Bootloader-ota 项目移植到新芯片平台的标准流程。本项目采用三层架构
（`lib/` 平台无关库、`os/` RTOS 内核、`platforms/` 平台实现），移植时
**lib/ 与 os/ 不动**，只在 `platforms/<chip>/` 下新建平台目录。

## 触发场景

- 用户要求移植/适配/迁移到新芯片（如"移植到 GD32"、"适配 ESP32"、"加个新平台"）
- 用户复制了 template 目录并开始填空
- 用户询问新芯片需要实现哪些接口
- 新平台构建失败，需要对照契约排查

## 前置：三层架构认知

```
lib/            平台无关 OTA 库（fsm/md5/lzma/module-manager/update-table/hal 接口）
os/freertos/    FreeRTOS 内核源码 + 头文件（不含 port）
platforms/
  host-sim/     Linux 仿真（POSIX port + host HAL）
  stm32f103/    STM32F103C8T6 参考实现（已跑通）
  template/     新平台模板（复制起点）
  <chip>/       新芯片（本工作流产物）
```

## 工作流步骤

### 第 1 步：复制模板

```bash
cp -r platforms/template platforms/<chip>
```

阅读 `platforms/template/README.md`，它列出了完整契约与移植步骤。
全程对照 `platforms/stm32f103/` 参考实现。

### 第 2 步：核对 5 个必须提供的接口符号

lib/ 平台无关代码通过固定符号名与平台链接，**符号名不可更改**，否则链接失败：

| 符号 | 声明位置（lib 提供） | 平台实现文件 | 用途 |
|------|---------------------|-------------|------|
| `hal_ota_t hal_ota_instance` | `lib/hal/hal_ota.h` | `drivers/hal_ota_impl.c` | App：FSM 调 `system_reset()` |
| `firmware_source_t g_spiflash_source` | `lib/hal/firmware_source.h` | `drivers/firmware_source_impl.c` | App+Boot：读固件源 |
| `ISzAlloc g_McuAlloc` | `lib/lzma/sdk/7zTypes.h` | `drivers/lzma_alloc.c` | LZMA 静态分配器 |
| `boot_read/write_upgrade_flag()` | `bootloader/boot_protocol.h` | `bootloader/boot_protocol.c` | 升级标志 |
| `printf_lite()` + `printf` 宏 | lib/app/printf.h（!HOST_SIM 拉入） | `printf_lite.c/.h` | 日志重定向 |

**关键检查点**：
- `lib/app/ota_main.c` 的 MCU 路径（`ota_app_start()`）直接引用 `g_spiflash_source`
  和 `hal_ota_instance` —— 必须用这两个名字。
- FreeRTOS 钩子 `vAssertCalled`/`vApplicationMallocFailedHook`/
  `vApplicationStackOverflowHook` 已由 `lib/app/ota_main.c` MCU 路径提供，平台**不要**重复实现。

### 第 3 步：实现 3 个底层驱动契约

模板用 3 个头文件定义驱动接口，平台在对应 `.c` 中按芯片手册实现寄存器操作：

| 契约头 | 必须实现的函数 | 用途 |
|--------|--------------|------|
| `drivers/board_uart.h` | `board_uart_init/putc/getc_nonblock/getc_timeout` | 日志 + UART 固件下载 |
| `drivers/board_storage.h` | `board_storage_init/read/write/erase_range/write_with_erase` | 外部存储（固件+标志） |
| `drivers/board_flash.h` | `board_flash_unlock/lock/erase_range/write_word` | 内部 Flash 擦写（仅 Bootloader） |

搜索平台源码中的 `TODO` 定位所有需填写处。

### 第 4 步：填平台特定代码

1. **`drivers/hal_ota_impl.c`**：实现 `board_system_reset()`（Cortex-M 写
   `SCB->AIRCR`；RISC-V 写看门狗/软复位寄存器）。Flash 操作在 App 中是桩（返回 false）。
2. **`bootloader/boot_protocol.h`**：改分区地址宏
   （`APP_BASE_ADDR`/`APP_MAX_SIZE`/`STORAGE_FLAG_OFFSET`），必须与链接脚本一致。
3. **`app/FreeRTOSConfig.h`**：改 `configCPU_CLOCK_HZ`、`__NVIC_PRIO_BITS`、
   `configTOTAL_HEAP_SIZE`。`configUSE_TIMERS=0` 时不要把 `timers.c` 加入构建。
4. **`bootloader/main.c` 的 `jump_to_app()`**：Cortex-M 版本已给出（MSP+VTOR+复位向量）；
   其他架构按 ABI 改写。若芯片 Flash 最小编程单位不是 4 字节，改 `TAIL_UNIT`。
5. **`app/main.c`**：在注释 TODO 处填时钟初始化；其余 OTA 调用顺序已固定。

### 第 5 步：准备 FreeRTOS 移植层与启动文件（仅 App 需要 RTOS）

- 从 FreeRTOS 官方源码取对应架构 port（`portable/GCC/<arch>/port.c` + `portmacro.h`），
  放到 `platforms/<chip>/freertos_port/`。
- 提供芯片 startup 汇编（向量表）+ 系统时钟初始化 + 链接脚本。
- 链接脚本需支持 Bootloader/App 两分区：Bootloader 用
  `--defsym FLASH_ORIGIN/FLASH_LENGTH` 覆盖起始与长度（参考 stm32f103 的 .ld）。
- **路径深度注意**：`platforms/<chip>/app/` 距项目根 3 层，引用 lib/os 用
  `../../../lib`、`../../../os`（不是 `../../`）。

### 第 6 步：更新 Makefile

编辑 `app/Makefile` 与 `bootloader/Makefile`：
- `CC`：交叉工具链前缀（如 `arm-none-eabi-gcc`）
- `CPUFLAGS`：`-mcpu`/`-mthumb`/`-mfloat-abi` 等
- `LDSCRIPT`：本芯片链接脚本
- `SRCS`：替换 startup/CMSIS/中断/syscalls/port 文件，保留 lib/os 的共享源
- 确保 **不加 `-DHOST_SIM`**（MCU 走 firmware_source + printf_lite 路径）
- Bootloader 加 `-DZ7_CRC_NUM_TABLES=1`（省 RAM），不含 Alloc.c、不含 FreeRTOS

### 第 7 步：构建验证

```bash
# App（应生成 build/app.elf + app.bin，并打印 size）
cd platforms/<chip>/app && make clean && make
# Bootloader（应生成 build/boot.elf + boot.bin）
cd platforms/<chip>/bootloader && make clean && make
# 回归仿真不受影响
cd ../../.. && make regression
```

**验收标准**：
- App/Bootloader 均编译链接通过，无 undefined reference
- size 输出在 Flash/RAM 容量内（对照 stm32f103：App text~20KB，Boot text~14KB）
- `make regression` 仍 PASS（确认 lib/os 未被误改）

## 常见问题

- **`undefined reference to g_spiflash_source/hal_ota_instance`**：符号名被改或对应 .c 未加入 SRCS。
- **`'firmware_source.h' file not found`**：Makefile 缺 `-I../../../lib` 与 `-I../../../lib/hal`；
  注意 app/bootloader 目录到根是 3 层（`../../../`）。
- **`'7zTypes.h' file not found`**：缺 `-I../../../lib/lzma/sdk`。
- **`'FreeRTOS.h' file not found`**：缺 `-I../../../os/freertos/include`。
- **链接了 newlib vfprintf（Flash 暴涨 ~20KB）**：`printf_lite.h` 必须先于 `printf.h`
  被包含；检查 include 顺序，或确认 `printf.h` 在 !HOST_SIM 时已拉入 printf_lite.h。
- **Bootloader 升级后跑飞**：检查 `jump_to_app()` 的 VTOR/MSP 设置；App 链接地址
  必须等于 `APP_BASE_ADDR`。
- **CRC32 链接错误（ARM）**：`lzma_alloc.c` 末尾的 `CPU_IsSupported_CRC32` stub
  返回 0 强制软件表 CRC；Cortex-M33+ 支持硬件 CRC 可改为返回 1。

## 关键参考文件

- 模板说明：`platforms/template/README.md`
- 完整参考实现：`platforms/stm32f103/`（app/bootloader/drivers/freertos_port）
- HAL 接口定义：`lib/hal/hal_ota.h`、`lib/hal/firmware_source.h`
- OTA 入口：`lib/app/ota_main.c`（`ota_app_start()` 与平台钩子）
- 链接脚本：`platforms/stm32f103/STM32F103C8Tx_FLASH.ld`
