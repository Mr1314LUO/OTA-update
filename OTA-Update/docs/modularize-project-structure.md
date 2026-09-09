# 模块化项目结构重构

## Context

当前项目将平台无关的 OTA 逻辑（`OTA-Update/`）与 STM32 MCU 代码（`mcu/`）混合存放，存在以下问题：

* `OTA-Update/hal/hal_ota.c` 是 host-sim 专用的 HAL 实现，却放在共享库目录中

* `OTA-Update/module/freertos/` 混合了 FreeRTOS 内核源码（共享）与 POSIX port（host-sim 专用）

* 每个 Makefile 都有 7-10 个跨目录的 `-I` 路径，依赖关系不清晰

* 迁移到新芯片时，无法一眼看出哪些目录需要替换、哪些保持不动

重构目标：将代码分为三层 —— **平台无关库**(`lib/`)、**RTOS 内核**(`os/`)、**平台实现**(`platforms/`)，迁移新芯片只需新建 `platforms/<chip>/` 目录。

## 目标结构

```
Bootloader-ota/
├── Makefile                      # 根 Makefile：host-sim 构建 + regression
├── README.md
├── .gitignore
│
├── lib/                          # 平台无关 OTA 库（迁移时不改）
│   ├── Makefile                  # 独立工具构建（firmware_create / flow-unzip / unzip / md5sum）
│   ├── hal/                      # HAL 接口头文件（无实现）
│   │   ├── hal_ota.h
│   │   └── firmware_source.h
│   ├── app/                      # 共享 OTA 入口逻辑（#ifdef HOST_SIM 条件编译）
│   │   ├── ota_main.c
│   │   ├── ota_main.h
│   │   └── printf.h
│   ├── fsm/                      # OTA 状态机
│   ├── update-table/             # 分区表
│   ├── module-manager/           # 模块管理
│   ├── md5/                      # MD5 校验
│   └── lzma/                     # LZMA 压缩/解压
│       ├── sdk/                  # LZMA SDK（vendored）
│       ├── zip/
│       ├── unzip/
│       └── flow-unzip/
│
├── os/                           # RTOS 内核（迁移时不改）
│   └── freertos/                 # FreeRTOS 内核源码 + 头文件（无 port）
│       ├── include/
│       ├── list.c / queue.c / tasks.c / timers.c / event_groups.c / heap_4.c
│
├── platforms/                    # 平台实现（迁移时新建对应目录）
│   ├── host-sim/                 # Linux 主机仿真
│   │   ├── Makefile              # host-sim OTA 构建
│   │   ├── hal/                  # host-sim HAL 实现
│   │   │   └── hal_ota.c
│   │   ├── freertos/             # FreeRTOS POSIX port
│   │   │   ├── port.c / portmacro.h
│   │   │   └── utils/
│   │   └── FreeRTOSConfig.h
│   │
│   └── stm32f103/                # STM32F103C8T6 平台
│       ├── app/                  # App 固件
│       ├── bootloader/           # Bootloader 固件
│       ├── drivers/              # STM32 外设驱动 + MCU HAL 实现
│       ├── cmsis/                # CMSIS
│       ├── freertos_port/        # FreeRTOS ARM Cortex-M3 port
│       ├── printf_lite.c/.h
│       ├── startup_stm32f103xb.s
│       └── STM32F103C8Tx_FLASH.ld
│
├── firmware-update/              # 预编译二进制（不变）
└── docs/                         # 文档（不变）
```

## 关键设计决策

1. **#include 语句无需修改**：当前 include 模式为 `subdir/file.h`（如 `"hal/hal_ota.h"`、`"md5/md5.h"`、`"lzma/zip/zip.h"`），通过 `-Ilib` 即可解析。`"printf.h"` 通过 `-Ilib/app` 解析。`"FreeRTOS.h"` 通过 `-Ios/freertos/include` 解析。跨平台 include（如 `ota_fsm.c` 中的 `"boot_protocol.h"`）均在 `#ifndef HOST_SIM` 守卫内，host-sim 编译时跳过。

2. **`module/`** **层级扁平化**：`OTA-Update/module/fsm/` → `lib/fsm/`、`OTA-Update/module/md5/` → `lib/md5/` 等。所有 `"fsm/ota_fsm.h"`、`"md5/md5.h"` 等 include 通过 `-Ilib` 解析，无需改 include 语句。

3. **FreeRTOSConfig.h 归平台**：host-sim 的 `FreeRTOSConfig.h` 移至 `platforms/host-sim/`，MCU 的已在 `platforms/stm32f103/app/`（原 `mcu/app/`）。FreeRTOS 内核通过 `#include "FreeRTOSConfig.h"` 引用，由平台 `-I` 路径提供。

4. **ota\_main.c 留在 lib/app/**：虽然它含 `#ifndef HOST_SIM` 块引用 MCU 驱动头（`"stm32f1_spi.h"` 等），但这些 include 被条件守卫，host-sim 编译时跳过。MCU 编译时平台 Makefile 的 `-I../drivers` 提供这些头文件。这比拆分为两个文件更简洁。

5. **lib/Makefile 只构建独立工具**：原 `OTA-Update/Makefile` 的 `$(TARGET_OTA)` 委托根 Makefile 的循环依赖去除。`lib/Makefile` 只构建 firmware\_create / flow-unzip / unzip / md5sum 四个独立工具。

## 实施步骤

### 步骤 1：创建目录结构

```bash
mkdir -p lib os/freertos
mkdir -p platforms/host-sim/hal platforms/host-sim/freertos/utils
mkdir -p platforms/stm32f103
```

### 步骤 2：移动 lib/（平台无关库）

从 `OTA-Update/` 移动，扁平化 `module/` 层级：

| 源路径                                 | 目标路径                        |
| ----------------------------------- | --------------------------- |
| `OTA-Update/hal/hal_ota.h`          | `lib/hal/hal_ota.h`         |
| `OTA-Update/hal/firmware_source.h`  | `lib/hal/firmware_source.h` |
| `OTA-Update/app/ota_main.c`         | `lib/app/ota_main.c`        |
| `OTA-Update/app/ota_main.h`         | `lib/app/ota_main.h`        |
| `OTA-Update/app/printf.h`           | `lib/app/printf.h`          |
| `OTA-Update/module/fsm/`            | `lib/fsm/`                  |
| `OTA-Update/module/md5/`            | `lib/md5/`                  |
| `OTA-Update/module/lzma/`           | `lib/lzma/`                 |
| `OTA-Update/module/module-manager/` | `lib/module-manager/`       |
| `OTA-Update/module/update-table/`   | `lib/update-table/`         |

**注意**：`OTA-Update/hal/hal_ota.c` 不移入 `lib/hal/`（它是 host-sim 实现，移到 `platforms/host-sim/hal/`）。

### 步骤 3：移动 os/freertos/（FreeRTOS 内核）

从 `OTA-Update/module/freertos/` 移动内核源码和头文件（不含 port、不含 config）：

| 源路径                                         | 目标路径                         |
| ------------------------------------------- | ---------------------------- |
| `OTA-Update/module/freertos/include/`       | `os/freertos/include/`       |
| `OTA-Update/module/freertos/list.c`         | `os/freertos/list.c`         |
| `OTA-Update/module/freertos/queue.c`        | `os/freertos/queue.c`        |
| `OTA-Update/module/freertos/tasks.c`        | `os/freertos/tasks.c`        |
| `OTA-Update/module/freertos/timers.c`       | `os/freertos/timers.c`       |
| `OTA-Update/module/freertos/event_groups.c` | `os/freertos/event_groups.c` |
| `OTA-Update/module/freertos/heap_4.c`       | `os/freertos/heap_4.c`       |

### 步骤 4：移动 platforms/host-sim/（host-sim 平台）

| 源路径                                            | 目标路径                                      |
| ---------------------------------------------- | ----------------------------------------- |
| `OTA-Update/hal/hal_ota.c`                     | `platforms/host-sim/hal/hal_ota.c`        |
| `OTA-Update/module/freertos/posix/port.c`      | `platforms/host-sim/freertos/port.c`      |
| `OTA-Update/module/freertos/posix/portmacro.h` | `platforms/host-sim/freertos/portmacro.h` |
| `OTA-Update/module/freertos/posix/utils/`      | `platforms/host-sim/freertos/utils/`      |
| `OTA-Update/module/freertos/FreeRTOSConfig.h`  | `platforms/host-sim/FreeRTOSConfig.h`     |

### 步骤 5：移动 platforms/stm32f103/（STM32 平台）

将 `mcu/` 内容整体移至 `platforms/stm32f103/`，保持子目录结构：

```bash
mv mcu/app      platforms/stm32f103/app
mv mcu/bootloader platforms/stm32f103/bootloader
mv mcu/cmsis    platforms/stm32f103/cmsis
mv mcu/drivers  platforms/stm32f103/drivers
mv mcu/freertos_port platforms/stm32f103/freertos_port
mv mcu/printf_lite.c platforms/stm32f103/printf_lite.c
mv mcu/printf_lite.h platforms/stm32f103/printf_lite.h
mv mcu/startup_stm32f103xb.s platforms/stm32f103/startup_stm32f103xb.s
mv mcu/STM32F103C8Tx_FLASH.ld platforms/stm32f103/STM32F103C8Tx_FLASH.ld
```

### 步骤 6：删除空目录

```bash
rm -rf OTA-Update   # 移动完成后整个目录应为空（或只剩空 module/freertos 等）
rm -rf mcu           # 已全部移走
```

### 步骤 7：重写根 Makefile

更新源文件路径和 include 路径。核心变更：

```makefile
# 头文件搜索路径
CPPFLAGS += -Ilib \
                -Ilib/app \
                -Ilib/hal \
                -Ilib/lzma/sdk \
                -Ios/freertos/include \
                -Iplatforms/host-sim \
                -Iplatforms/host-sim/freertos \
                -D_POSIX_C_SOURCE=200809L \
                -D_DEFAULT_SOURCE \
                -DUSE_FREERTOS \
                -DHOST_SIM

# 源文件
SRCS := lib/app/ota_main.c \
        platforms/host-sim/hal/hal_ota.c \
        lib/fsm/ota_fsm.c \
        lib/update-table/firmware_update.c \
        lib/module-manager/module_manager.c \
        lib/md5/md5.c \
        lib/lzma/unzip/unzip.c \
        lib/lzma/zip/zip.c \
        lib/lzma/flow-unzip/unzip_stream.c \
        lib/lzma/sdk/Alloc.c \
        lib/lzma/sdk/LzmaDec.c \
        lib/lzma/sdk/7zCrc.c \
        lib/lzma/sdk/7zCrcOpt.c \
        os/freertos/list.c \
        os/freertos/queue.c \
        os/freertos/tasks.c \
        os/freertos/timers.c \
        os/freertos/event_groups.c \
        os/freertos/heap_4.c \
        platforms/host-sim/freertos/port.c \
        platforms/host-sim/freertos/utils/wait_for_event.c

VPATH := lib/app:lib/hal:lib/fsm:lib/update-table:lib/module-manager:lib/md5: \
         lib/lzma/unzip:lib/lzma/zip:lib/lzma/flow-unzip:lib/lzma/sdk: \
         os/freertos: \
         platforms/host-sim/hal:platforms/host-sim/freertos:platforms/host-sim/freertos/utils
```

regression / run / clean 目标逻辑不变，仅路径跟随 `BUILD_DIR` 变量自动更新。

### 步骤 8：重写 lib/Makefile（独立工具）

从原 `OTA-Update/Makefile` 改写，去掉 `$(TARGET_OTA)` 委托目标，扁平化路径：

```makefile
APP_DIR    := app
SDK_DIR    := lzma/sdk

INCLUDES := -I. -I$(APP_DIR) -I$(SDK_DIR)

# 源路径从 module/xxx/ 变为 xxx/
ZIP_SRCS  := lzma/zip/zip.c
SDK_SRCS  := $(SDK_DIR)/7zCrc.c $(SDK_DIR)/7zCrcOpt.c $(SDK_DIR)/Alloc.c $(SDK_DIR)/LzmaDec.c
UNZIP_STREAM_SRCS := lzma/flow-unzip/unzip_stream.c
UNZIP_SRCS := lzma/unzip/unzip.c
MD5_SRCS  := md5/md5.c
# ... 其余规则类同，路径中 module/ 前缀去掉
```

### 步骤 9：更新 platforms/stm32f103/app/Makefile

将所有 `../../OTA-Update/` 引用改为 `../../../lib/` 和 `../../../os/`：

```makefile
CPPFLAGS = -I.. -I../cmsis -I../drivers -I../freertos_port -I. -I../bootloader \
           -I../../lib -I../../lib/app -I../../lib/lzma/sdk \
           -I../../lib/hal \
           -I../../os/freertos -I../../os/freertos/include
```

vpath 路径同步更新。源文件路径中 `../../OTA-Update/` → `../../lib/` 或 `../../os/`。

### 步骤 10：更新 platforms/stm32f103/bootloader/Makefile

同上，将 `../../OTA-Update/` 引用改为 `../../lib/`：

```makefile
CPPFLAGS = -I.. -I../cmsis -I../drivers -I. \
           -I../../lib -I../../lib/app -I../../lib/lzma/sdk \
           -I../../lib/lzma/flow-unzip -I../../lib/hal
```

源文件路径中 `../../OTA-Update/module/lzma/` → `../../lib/lzma/`。

### 步骤 11：更新 .gitignore

如果 `.gitignore` 引用了旧路径，更新为新路径。

## 验证

1. **host-sim 构建 + 回归测试**：

   ```bash
   make clean && make regression
   ```

   预期：`P8 PASS: Summary Failed:0, 已到达 SUCCESS`，与重构前一致。

2. **独立工具构建**：

   ```bash
   cd lib && make clean && make
   ```

   预期：生成 firmware\_create / flow-unzip / unzip / md5sum 四个二进制。

3. **MCU App 构建**（如有交叉工具链）：

   ```bash
   cd platforms/stm32f103/app && make clean && make
   ```

   预期：生成 `build/app.elf` 和 `build/app.bin`，size 与重构前一致。

4. **MCU Bootloader 构建**：

   ```bash
   cd platforms/stm32f103/bootloader && make clean && make
   ```

   预期：生成 `build/boot.elf` 和 `build/boot.bin`，size 与重构前一致。

5. **验证无残留**：确认 `OTA-Update/` 和 `mcu/` 目录已删除，无残留文件。

