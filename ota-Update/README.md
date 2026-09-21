<<<<<<< HEAD
# OTA-update
=======
# Bootloader-ota — 嵌入式 OTA 固件升级框架

面向嵌入式设备（Bootloader 侧）的 OTA（Over-The-Air）固件升级框架。采用**表格驱动有限状态机（FSM）**组织升级流程，通过 **HAL 硬件抽象层**隔离 Flash 操作，集成 **LZMA 流式压缩/解压**、**MD5 + CRC32 双重完整性校验**、**分区表多模块升级**与**升级清单解析**。

当前工程可在 Linux 主机上直接编译运行：HAL 层提供基于 `usleep` 的桩实现来模拟 STM32 Flash 擦写时序，便于在 PC 上开发、调试与验证完整升级链路；移植到真实硬件时只需替换 HAL 接口实现。

---

## 功能特性

- **表格驱动状态机**：状态/事件/转移规则集中在一张转移表中，新增状态或事件只需增删表项，逻辑清晰、易维护。
- **HAL 硬件抽象**：`flash_erase / flash_write / flash_read / system_reset` 以函数指针接口暴露，主机端为桩实现，真实设备对接芯片 HAL 库即可。
- **LZMA 固件压缩**：升级包以 LZMA 压缩，显著减小传输体积（演示固件压缩率约 44.7%）。
- **真流式升级**：解压数据逐块直接写入 Flash，不落盘、不占用大块 RAM，适配资源受限的 MCU。
- **完整性校验**：下载后做 MD5 校验，解压写 Flash 时做固件头魔数与 CRC32 校验。
- **分区表多模块升级**：按 `boot / hal / bsp / app / module / nvs / firmware` 等分区表项，依优先级顺序升级，缺失文件自动跳过。
- **升级清单解析**：行式 `key=value` 清单，描述模块名、当前/目标版本、是否强制升级等。
- **彩色日志系统**：`printf.h` 提供分级日志、源码位置（文件:行号:函数，终端可点击跳转）、进度刷新与 Emoji 状态图标。
- **可交叉编译**：主机用 `gcc`，嵌入式可切换为 `arm-none-eabi-gcc` 等工具链。

---

## 目录结构

```
Bootloader-ota/
├── Makefile                              # 主构建脚本（gcc / 可改交叉编译）
├── .gitignore
├── README.md                             # 本文档
├── OTA-Update/
│   ├── printf.h                          # 彩色分级日志框架（LOG_INFO/ERROR/WARN/DEBUG…）
│   ├── app/
│   │   └── ota_main.c                    # 程序入口：初始化 HAL 与状态机，主循环驱动
│   ├── hal/
│   │   ├── hal_ota.h                     # HAL 接口定义（Flash 擦写读 + 系统复位）
│   │   └── hal_ota.c                     # 主机端桩实现（模拟 STM32 Flash 时序）
│   └── module/
│       ├── fsm/
│       │   ├── ota_fsm.h                 # 状态/事件/上下文/转移表接口
│       │   └── ota_fsm.c                 # 表格驱动状态机实现 + 各状态动作函数
│       ├── module-manager/
│       │   ├── module_manager.h          # 模块管理接口
│       │   └── module_manager.c          # 升级清单解析、模块查询、是否有更新
│       ├── update-table/
│       │   ├── firmware_update.h         # 分区表/固件模块元数据/升级接口
│       │   └── firmware_update.c         # 分区表驱动升级：擦除 + 流式解压写 Flash
│       ├── md5/
│       │   ├── md5.h / md5.c             # MD5 算法实现与文件校验
│       │   └── md5sum.sh                 # MD5 生成/校验辅助脚本
│       └── lzma/
│           ├── README.md                 # LZMA 模块详细说明
│           ├── build_lzma.sh             # LZMA 模块独立构建脚本
│           ├── sdk/                      # LZMA SDK（LzmaDec / 7zCrc / Alloc 等，公共领域）
│           ├── zip/                      # 固件打包压缩 compressed_File()
│           ├── unzip/                    # 固件解压（基于系统 liblzma）
│           └── flow-unzip/               # 流式解压 perform_firmware_update_stream()
└── firmware-update/                      # 升级包目录（主机模拟用）
    ├── firmware.bin                      # 原始固件（下载/校验对象）
    ├── firmware.bin.lzma                 # LZMA 压缩升级包（含 32 字节固件头）
    └── firmware.bin.md5                  # firmware.bin 的 MD5 校验文件
```

---

## 系统架构与升级流程

### 分层结构

```
┌─────────────────────────────────────────────┐
│  app/ota_main.c        入口 + 主循环驱动      │
├─────────────────────────────────────────────┤
│  module/fsm            表格驱动状态机（核心）  │
├──────────────┬──────────────┬───────────────┤
│ module-manager│ update-table │     md5       │
│  清单解析/模块 │  分区表升级   │  完整性校验    │
├──────────────┴──────────────┴───────────────┤
│  module/lzma   zip / unzip / flow-unzip /sdk │
├─────────────────────────────────────────────┤
│  hal/hal_ota    Flash 擦写读 + 系统复位（抽象）│
└─────────────────────────────────────────────┘
```

### OTA 状态机

状态转移由事件驱动，转移规则定义在 [ota_fsm.c](OTA-Update/module/fsm/ota_fsm.c) 的 `ota_state_table[]` 中：

```
                  START_CHECK
IDLE ───────────────────────────► CHECKING
                                   │  MANIFEST_SUCCESS → 解析清单
                          ┌────────┴────────┐
                   NO_UPDATE│                 │UPDATE_AVAILABLE
                          ▼                  ▼
                        IDLE          DOWNLOADING
                                          │ DOWNLOAD_COMPLETE
                                          ▼
                                      VERIFYING ──VERIFY_FAILED──► FAILED
                                          │ VERIFY_SUCCESS
                                          ▼
                                        READY ──UPDATE_FAILED───► FAILED
                                          │ READY_CONFIRM
                                          ▼
                                      UPDATING ──UPDATE_FAILED──► FAILED
                                          │ UPDATE_COMPLETE
                                          ▼
                                       SUCCESS
```

| 状态 | 说明 | 关键动作 |
|------|------|----------|
| `IDLE` | 空闲，等待触发 | 自动发起 `EVENT_START_CHECK` |
| `CHECKING` | 检查更新 | 获取并解析升级清单，判断是否有更新 |
| `DOWNLOADING` | 下载升级包 | 分块接收（主机模拟下载进度） |
| `VERIFYING` | 校验升级包 | 对 `firmware.bin` 做 MD5 完整性校验 |
| `READY` | 准备就绪 | 将固件压缩打包为 `.lzma`，等待确认 |
| `UPDATING` | 正在升级 | 遍历分区表，擦除 Flash 并流式解压写入 |
| `SUCCESS` | 升级成功 | 记录结果（真实设备随后复位引导新固件） |
| `FAILED` | 升级失败 | 记录错误信息，可通过 `EVENT_START_CHECK` 重试 |

> 主机模拟下，网络清单、下载、用户确认等环节均以本地文件/自动触发代替；代码注释中标注了真实设备对应的异步事件接入点。

---

## 固件包格式

LZMA 升级包（`.lzma`）由 32 字节固件头 + LZMA 数据组成：

```
┌──────────────────────────────┬──────────────────────┬─────────────────────┐
│  FirmwareHeader (32 bytes)   │ LZMA Properties (5B) │ LZMA Compressed Data│
└──────────────────────────────┴──────────────────────┴─────────────────────┘
```

固件头 `FirmwareHeader_t`（定义于 [zip.h](OTA-Update/module/lzma/zip/zip.h)、[unzip_stream.h](OTA-Update/module/lzma/flow-unzip/unzip_stream.h)）：

| 字段 | 类型 | 说明 |
|------|------|------|
| `magic` | uint32 | 魔数 `0x46575246`（ASCII `"FRWF"`），用于识别固件包 |
| `version` | uint32 | 数值版本号，编码为 `(主版本 << 16) \| 次版本` |
| `compressed_size` | uint32 | 压缩数据大小 |
| `uncompressed_size` | uint32 | 未压缩数据大小 |
| `crc32` | uint32 | 未压缩数据的 CRC32 校验值 |
| `version_str[12]` | char[] | 版本字符串，如 `"V2.0"` |

版本字符串与数值版本号可通过 `fw_version_encode()` / `fw_version_to_str()` / `fw_version_display()` 互转。

---

## 升级清单格式

清单为行式 `key=value` 文本，`#` 开头为注释，每个模块以 `module=<名称>` 开始（见 [module_manager.c](OTA-Update/module/module-manager/module_manager.c)）：

```ini
# OTA upgrade manifest
module=firmware
version=V1.0
target_version=V2.0
required=1
```

| 字段 | 说明 |
|------|------|
| `module` | 模块名（如 firmware、app、bootloader） |
| `version` | 当前版本 |
| `target_version` | 目标版本（会写入固件包头部） |
| `size` | 模块大小（可选） |
| `required` | 是否强制升级（`1`/`0`） |

主机模拟使用内置演示清单 `demo_manifest`；真实设备由服务器通过 HTTP/MQTT 下发。

---

## 分区表

分区表 `firmware_partition_table[]` 定义在 [firmware_update.c](OTA-Update/module/update-table/firmware_update.c)，升级时按 `priority`（数字越小越先）顺序处理：

| 模块 | 固件文件 | Flash 起始地址 | 最大容量 | 优先级 |
|------|----------|---------------|----------|--------|
| boot | boot.bin | 0x08000000 | 12 KB | 1 |
| hal | hal.bin | 0x08050000 | 128 KB | 2 |
| bsp | bsp.bin | 0x08070000 | 64 KB | 3 |
| app | app.bin | 0x08010000 | 256 KB | 3 |
| module | module.bin | 0x08020000 | 1024 KB | 4 |
| nvs | nvs.bin | 0x08080000 | 64 KB | 5 |
| firmware | firmware.bin.lzma | 0x08000000 | 8 MB | 6 |

> 分区地址、容量、优先级可按实际 Flash 布局增删修改。升级时若某固件文件不存在则跳过并告警；`.lzma` 包走流式解压写 Flash，普通 `.bin` 走分块直写。

---

## 编译与运行

### 环境依赖

- **编译器**：gcc（C11）；交叉编译可改用 `arm-none-eabi-gcc` 等
- **系统库**：`liblzma`（`unzip.c` 链接 `-llzma`）
  - Debian/Ubuntu 安装：`sudo apt-get install gcc liblzma-dev`
- **主机环境**：Linux/POSIX（桩实现用到 `usleep`、`dirent.h`、`sys/stat.h`）
- LZMA SDK 源码已随仓库内置在 `OTA-Update/module/lzma/sdk/`，无需额外下载

### 构建命令

```bash
make            # 编译并运行（all 目标：构建后直接执行 ./build/ota_main）
make run        # 编译并运行
make build/ota_main   # 仅编译链接，不运行
make clean      # 清理 build/ 构建产物
```

构建产物统一输出到 `build/`，目标程序为 `build/ota_main`。

> 程序主循环为嵌入式风格的 `while(1)`，到达 `SUCCESS`/`FAILED` 终态后驻留循环，主机下按 **Ctrl+C** 退出。主机桩实现用 `usleep` 模拟 Flash 擦写/下载耗时，完整跑一遍需要数秒到数十秒。

### 交叉编译

修改 [Makefile](Makefile) 顶部工具链，或通过命令行覆盖：

```bash
make CC=arm-none-eabi-gcc
```

> 交叉编译时 `unzip.c` 依赖目标平台的 liblzma；若目标平台无 liblzma，可仅使用基于内置 SDK 的 `flow-unzip` 流式解压路径，并在 Makefile 中剔除 `unzip.c` 与 `-llzma`。

---

## 工具脚本

### MD5 校验脚本

[md5sum.sh](OTA-Update/module/md5/md5sum.sh) 用于生成/校验固件 MD5：

```bash
# 生成校验文件（默认输出 <文件>.md5）
./OTA-Update/module/md5/md5sum.sh g firmware-update/firmware.bin

# 校验固件完整性
./OTA-Update/module/md5/md5sum.sh c firmware-update/firmware.bin
```

> 更换 `firmware.bin` 后，需重新生成 `firmware.bin.md5`，否则状态机 `VERIFYING` 阶段会校验失败。

### LZMA 独立构建脚本

[build_lzma.sh](OTA-Update/module/lzma/build_lzma.sh) 可单独编译 LZMA 相关目标文件到 `build/` 目录，用于独立验证压缩/解压模块。

---

## 移植到真实硬件

移植核心是**实现 HAL 接口**（[hal_ota.h](OTA-Update/hal/hal_ota.h)），将主机桩函数替换为芯片 SDK 调用：

```c
typedef struct {
    bool (*flash_erase)(uint32_t addr, uint32_t size,
                        erase_progress_cb_t progress_cb, const char *module_name);
    bool (*flash_write)(uint32_t addr, const uint8_t *data, uint32_t len);
    void (*flash_read) (uint32_t addr, uint8_t *data, uint32_t len);
    void (*system_reset)(void);
} hal_ota_t;
```

移植要点：

1. **Flash 擦除**：按芯片扇区/页大小擦除，建议每擦完一页通过 `progress_cb` 回调进度（参考主机实现按 2KB 页回调）。
2. **Flash 写入**：将解压输出块写入指定地址；流式升级路径会逐块（默认 4KB）调用本接口。
3. **Flash 读取**：用于回读校验等场景。
4. **系统复位**：升级成功后调用芯片软复位（如 STM32 的 `NVIC_SystemReset()`），由 Bootloader 引导新固件。
5. **网络接入**：在 `action_start_check` / `action_start_download` 等处，将主机模拟逻辑替换为 HTTP/MQTT 异步请求，收到响应/数据后触发对应 `EVENT_*` 事件。
6. **内存调优**：按 MCU RAM 调整 `INPUT_BUFFER_SIZE` / `OUTPUT_BUFFER_SIZE` / `FIRMWARE_UPDATE_BUF_SIZE`，必要时改用静态分配器（见 [lzma/README.md](OTA-Update/module/lzma/README.md)）。
7. **日志裁剪**：串口带宽紧张时可编译期关闭位置信息 `-DLOG_LOC_ENABLE=0` 或调低日志等级 `-DLOG_LEVEL=1`。

---

## 日志系统

[printf.h](OTA-Update/printf.h) 提供轻量彩色日志，输出格式为 `[级别] 文件:行号:函数 消息`，在 VSCode/Trae 终端中可 Ctrl+点击文件位置直接跳转源码：

```c
LOG_ERROR("升级失败: %s\n", msg);     // 红色错误
LOG_WARN("文件缺失，跳过 %s\n", name); // 黄色警告
LOG_INFO("开始下载 (%u bytes)\n", n);  // 绿色信息
LOG_DEBUG(...);                        // 青色调试（LOG_LEVEL>=4）
LOG_SUCCESS("升级成功\n");             // 成功 ✅
LOG_FAILURE("升级失败\n");             // 失败 ❌
LOG_RAW("\r进度: %d%%", pct);          // 无前缀，适合 \r 进度刷新
LOG_HEX(buf, 64);                      // 十六进制转储（调试级）
```

日志等级通过编译宏控制：`-DLOG_LEVEL=N`（0 关 / 1 错误 / 2 +警告 / 3 +信息（默认）/ 4 全部）。

---

## 许可证

- 工程自有代码：随项目协议发布。
- LZMA SDK（`OTA-Update/module/lzma/sdk/`）：来自 7-Zip 的 LZMA SDK，属**公共领域（Public Domain）**，可自由使用、修改与分发。
>>>>>>> 17b22a2 (特性：新增用于MD5校验和计算与验证的命令行工具)
