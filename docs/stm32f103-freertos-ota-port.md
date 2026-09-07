# STM32F103C8T6 FreeRTOS + OTA 移植规划

## Context

当前项目 `/home/buduojun/Projects/Module/Bootloader-ota` 的 FreeRTOS 跑在 Linux 主机 POSIX 仿真层上,OTA 流程基于文件系统(`fopen/fread/stat`),且分区表总和远超 64KB。用户要求将其移植到 STM32F103C8T6(Cortex-M3 / 64KB Flash / 20KB RAM),目标"内核 + 完整 OTA 任务跑起来",保留 LZMA 压缩、保留 MD5 但去文件依赖,下载源改为外部 SPI Flash。

**架构死结**:F103 单 bank Flash,App 不能边运行边擦写自己的运行区;20KB RAM 也不够"复制 App 到 RAM 执行升级"。采用**业界标准方案**:App 在 FreeRTOS 任务里只负责"准备升级请求 + 写元数据标志到外部 SPI Flash + 软复位",实际擦写由 Bootloader 接管(Bootloader 从 SPI Flash 流式读 .lzma + 解压 + 写 App 区 + 跳转)。这样 FreeRTOS 上"完整 OTA 任务跑起来" = 状态机全流程到 UPDATING→复位由 Bootloader 完成实际擦写→跳回新 App 看到成功状态,且断电安全。

## 目标内存映射

```
内部 Flash 64KB @ 0x08000000
├── Bootloader 16KB  0x08000000-0x08003FFF  (无 RTOS,复位跳转+接管升级)
└── App         48KB 0x08004000-0x0800FFFF   (FreeRTOS + OTA 任务)
外部 SPI Flash (W25Qxx on SPI1: PA5/6/7 + CS PA4)
├── 0x000000  Firmware 镜像区  (.lzma 固件包, 预先烧录)
└── 0x080000  OTA 元数据区    (升级标志/期望 MD5/版本/大小, 4KB 扇区)
```

## 关键事实(已验证)

| 项 | 现值 | 来源 |
|---|---|---|
| LZMA probs 默认 lc=3 | ~15.9KB,RAM 杀手 | [LzmaDec.c:157,162,164](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/lzma/sdk/LzmaDec.c) |
| LZMA probs lc=0/lp=0 | ~5.4KB,可行 | 同上 |
| unzip_stream 栈缓冲 8KB | 直接撑爆任务栈 | [unzip_stream.c:74-75](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/lzma/flow-unzip/unzip_stream.c) |
| Alloc.c 拉 unistd/mman | 不能上 MCU | `Alloc.c:181,189,197,366-516,489-603` |
| 当前 FreeRTOS heap | 1MB | [FreeRTOSConfig.h:37](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/freertos/FreeRTOSConfig.h) |
| 任务栈深 4096 | POSIX 占位 | [ota_main.c:168-170](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/app/ota_main.c) |
| FSM 文件依赖 | stat/compare_flie/compressed_File | [ota_fsm.c:74-75,94,107](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/fsm/ota_fsm.c) |
| firmware_update 文件依赖 | fopen/fread/dir_exists/malloc(4KB) | [firmware_update.c:21-28,67-77,114-130,161](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/update-table/firmware_update.c) |
| F103C8 页大小 | 1KB(中容量,不是桩的 2KB) | — |

## 资源预算

**App 48KB Flash**:FreeRTOS 内核 8KB + ARM_CM3 port 1.5KB + FSM/update/md5/mananger 4KB + ota_main+printf_lite+syscalls 3KB + drivers(flash/spi/w25q/uart/gpio/hal_ota/it)3KB ≈ **19.5KB / 48KB**(富裕,可放业务)

**App 20KB RAM**:FreeRTOS heap 3KB + 任务栈(ota 0.5K+download 0.4K+confirm 0.25K+IDLE 0.5K)1.65KB + g_modules[8] 0.5KB + g_ota_ctx+队列/mutex 0.3KB + newlib/printf_lite BSS+栈 1.5KB ≈ **6.95KB / 20KB**(宽松,App 不做 LZMA 解码)

**Bootloader 16KB Flash**(承担 LZMA 解码):LZMA SDK 9KB + unzip_stream 2KB + drivers 3KB + startup/system/syscalls 1.5KB ≈ **15.5KB / 16KB** ⚠ 极紧。**后备方案**:扩 Bootloader 到 20KB,App 压到 44KB(改 [STM32F103C8Tx_FLASH.ld](file:///home/buduojun/Projects/Module/Bootloader-ota/mcu/STM32F103C8Tx_FLASH.ld) 的 `PROVIDE(FLASH_LENGTH)` 默认值,以及 [mcu/bootloader/Makefile](file:///home/buduojun/Projects/Module/Bootloader-ota/mcu/bootloader/Makefile) 的 `--defsym,FLASH_LENGTH=0x5000`,无需新建 .ld 文件)。

**Bootloader 20KB RAM**:LZMA arena(probs 5.4KB + dic 4KB,可省到 5.4KB)+ static in/out_buf 1KB + 临时 0.5KB + 主栈 2KB ≈ **9-13KB / 20KB** ✓

## 新目录结构

```
Bootloader-ota/
├── Makefile                              [保留] PC 仿真
├── OTA-Update/                           [改] 去文件依赖,共享于 PC/MCU
│   ├── hal/
│   │   ├── hal_ota.{c,h}                 [改] hal_ota.c L38 注释 2KB→1KB;MCU 不链接此 .c
│   │   └── firmware_source.h             [新] 固件源接口抽象
│   ├── module/
│   │   ├── fsm/ota_fsm.{c,h}             [改] 去 stat/compare_flie/compressed_File
│   │   ├── update-table/firmware_update.{c,h}  [改] 分区表重定义,去文件路径,缩 buf
│   │   ├── lzma/flow-unzip/unzip_stream.{c,h}  [改] 缓冲 static+缩小,去 fopen/fread
│   │   ├── lzma/zip/zip.c                [改] 加小字典 build flag(MCU_DICT)
│   │   ├── lzma/sdk/Alloc.c              [MCU 排除] 用 lzma_alloc 替
│   │   ├── md5/md5.{c,h}                [改] 增加 buffer/source MD5
│   │   └── module-manager/module_manager.h [改] 去 dirent.h/sys/stat.h
│   └── app/ota_main.{c,h}               [改] 栈深缩,钩子改 SPI Flash 元数据
├── mcu/                                  [新目录]
│   ├── app/                              App 固件 (48KB @ 0x08004000)
│   │   ├── Makefile
│   │   ├── FreeRTOSConfig.h             MCU 调优副本
│   │   ├── main.c                        App 入口(OTA 任务+UART 命令)
│   │   ├── stm32f1xx_it.c               SysTick/PendSV/SVC→port
│   │   └── syscalls.c                   newlib stubs(_write→UART)
│   ├── bootloader/                       Bootloader (16KB @ 0x08000000)
│   │   ├── Makefile
│   │   ├── main.c                        升级决策+跳转
│   │   └── boot_protocol.{c,h}         upgrade_flag 读写(SPI Flash 元数据,P7)
│   ├── cmsis/                            (自写最小化,不依赖完整 CMSIS)
│   │   ├── stm32f103xb.h                 设备头:SCB/NVIC/SysTick/RCC/FLASH/GPIO/SPI/USART
│   │   └── system_stm32f1xx.c            SystemInit(HSE×PLL9=72MHz,2 WS)
│   ├── drivers/
│   │   ├── stm32f1_flash.{c,h}          FLASH 解锁/擦页/编程/上锁,提供 flash_erase_range/write_word
│   │   ├── stm32f1_hal_ota.c            MCU hal_ota_t 实例(P4 桩,仅 system_reset;hal_ota_instance 符号在此)
│   │   ├── stm32f1_gpio.{c,h}           GPIOA/B 配置(SPI/UART/LED 引脚)
│   │   ├── stm32f1_spi.{c,h}            SPI1 轮询(PA5/6/7,CS PA4)
│   │   ├── stm32f1_uart.{c,h}           USART1(PA9/10) 115200 8N1 轮询 TX + 超时/非阻塞 RX
│   │   ├── w25qxx.{c,h}                 read-only: read(addr,buf,len)+read_id+erase_range
│   │   ├── firmware_source_spiflash.c  SPI Flash-backed firmware_source
│   │   └── lzma_alloc.{c,h}            静态 arena bump allocator,替 Alloc.c
│   ├── freertos_port/                    ARM_CM3 port (vendor,FreeRTOS 官方 GCC/ARM_CM3)
│   │   ├── port.c                        ← portable/GCC/ARM_CM3/port.c
│   │   └── portmacro.h                  ← portable/GCC/ARM_CM3/portmacro.h
│   ├── printf_lite.{c,h}                 轻量 printf(~1KB Flash),规避 newlib printf
│   ├── startup_stm32f103xb.s            启动文件(向量表+Reset_Handler)
│   └── STM32F103C8Tx_FLASH.ld           链接脚本(App/Boot 共用,ORIGIN/LENGTH 由 --defsym 区分)
└── firmware-update/firmware_mcu.bin.lzma [新] 主机端小字典重打包产物
```

## 关键修改清单(逐项,带行号)

### FreeRTOSConfig.h(MCU 副本 `mcu/app/FreeRTOSConfig.h`)
| 项 | 原值 | MCU 值 |
|---|---|---|
| `configMINIMAL_STACK_SIZE`(L20) | 512 | **128** |
| `configTOTAL_HEAP_SIZE`(L37) | 1MB | **3072**(3KB) |
| `configUSE_TIMERS`(L44) | 1 | **0**(省 timer 任务,ARM_CM3 port 不需要) |
| `configCHECK_FOR_STACK_OVERFLOW`(L53) | 0 | **2** |
| `configMAX_PRIORITIES`(L18) | 8 | 4 |
| 新增 | — | `configPRIO_BITS=4`、`configLIBRARY_LOWEST_INTERRUPT_PRIORITY=15`、`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY=5`、`configKERNEL_INTERRUPT_PRIORITY=0xFF` |

### [ota_fsm.c](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/fsm/ota_fsm.c)
- `action_start_download`(L70-88):`stat(CHECK_FILE_PATH)` → `ctx->total_size = fw_src->size(fw_src->ctx)`;错误分支检查 `total_size==0`。
- `action_start_verify`(L91-100):`compare_flie(CHECK_FILE_PATH, MD5_PATH)` → `source_md5(fw_src, digest)` 与元数据期望 MD5 对比。
- `action_prepare_update`(L103-116):**删除 `compressed_File` 调用**(L107);MCU 固件包在 SPI Flash 中已是 .lzma,无需打包;改为写 `upgrade_flag=MAGIC_PENDING` 到 SPI Flash 元数据区 → `ota_platform_request_confirm()`。
- `action_start_updating`(L119-128):MCU 路径改为置标志(上一步已置)+ `hal_ota_instance.system_reset()`(软复位,Bootloader 接管);PC 仿真保留 `firmware_update()`。
- `ota_fsm.h`(L7-10):删 `<sys/stat.h>` 与 `<unistd.h>`;`#include "lzma/zip/zip.h"` 改为条件编译(MCU 不需要)。

### [firmware_update.c](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/update-table/firmware_update.c)
- 分区表(L5-18)重定义:`static const firmware_partition_entry_t firmware_partition_table[] = {{"app", SPI_FLASH_FW_OFFSET, APP_BASE, APP_MAX_SIZE, 1}};`(单镜像升级)
- 删 `dir_exists`(L21-28)。
- `flash_firmware_from_file`(L61-158)→ `flash_firmware_from_source(entry, firmware_source_t* src)`:`fopen`→`src->read`;`fread`→`src->read`。
- `flashing_firmware`(L161-204):`fread`→`src->read`;`malloc(4KB)`→`static uint8_t buf[512]`。
- `firmware_update`(L207-258):`dir_exists`→删;`stat`→`src->size()` 判存在。
- `firmware_update.h`(L7-9):删 `<sys/stat.h>`;L15-17 路径宏→`SPI_FLASH_FW_OFFSET/META_OFFSET/APP_BASE/APP_MAX_SIZE/BOOT_BASE/BOOT_MAX_SIZE`;L27 `FIRMWARE_UPDATE_BUF_SIZE`→512;L73 `flashing_firmware` 签名改。

### [unzip_stream.c](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/lzma/flow-unzip/unzip_stream.c)
- `decompress_firmware_stream`(L66-188):
  - L74-75 栈 `in_buf[4KB]/out_buf[4KB]` → `static uint8_t in_buf[INPUT_BUFFER_SIZE]`、`static uint8_t out_buf[OUTPUT_BUFFER_SIZE]`;MCU 编译期 `-DINPUT_BUFFER_SIZE=512 -DOUTPUT_BUFFER_SIZE=512`。
  - 参数 `FILE *in_fp`(L67)→ `firmware_source_t* src`;`fread`(L87,104)→ `src->read(ctx, off, ...)`。
  - 保留 `write_fn` 回调(Bootloader 写 App flash;App PC 仿真写文件)。
- `perform_firmware_update_stream`(L201-299)拆出 `perform_firmware_update_stream_from_source(firmware_source_t*, ...)`:L217 `fopen`→用 src;L229 `fread(&header,...)`→`src->read(0,&header,sizeof)`;L224-226 `fseek/ftell`→`src->size()`;L268 `fclose` 删。
- `file_write_cb`(L26-34)+`perform_firmware_update`(L304-327)+`main`(L382-408)用 `#ifdef HOST_SIM` 包裹(MCU 不编 CLI)。
- `unzip_stream.h`(L87-88):`INPUT_BUFFER_SIZE/OUTPUT_BUFFER_SIZE` 加 `#ifndef` 保护;L95 `g_FirmwareAlloc` 加 `#ifdef TARGET_STM32 → g_McuAlloc`。

### [md5.c](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/md5/md5.c)
- 新增 `int buffer_md5(const uint8_t* data, size_t len, char* out)` 与 `int source_md5(firmware_source_t* src, char* out)`(分块读+`md5_update`)。`ota_fsm.c::action_start_verify` 调 `source_md5`。
- `file_md5`(L98-121)、`compare_flie`(L179-202)、`read_verify_file`(L137-167):`#ifdef HOST_SIM` 包裹。

### [ota_main.c](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/app/ota_main.c)
- `download_task`(L52-86):L54 `chunk=4096`→512;模拟分块→MCU 改为校验 SPI Flash 内固件就绪(读头校验 magic/size),完成发 `EVENT_DOWNLOAD_COMPLETE`。
- `vAssertCalled`(L110-113)、`vApplicationMallocFailedHook`(L115-118):`fprintf(stderr)/abort`→`LOG_ERROR`(UART)+`while(1)`。
- `main`(L142-186):L168-170 `xTaskCreate(...,4096,...)`→`512/384/256`(ota/download/confirm)。
- `ota_platform_request_confirm`(L135-137):MCU 实现可触发写 `upgrade_flag=MAGIC_PENDING` 到 SPI Flash 元数据,后续在 `action_start_updating` 调 `system_reset()`。

### [ota_fsm.h](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/fsm/ota_fsm.h)
- L9 `<sys/stat.h>` 删;L10 `<unistd.h>` 删。
- L14 `#include "lzma/zip/zip.h"` 改为 `#ifdef HOST_SIM` 包裹。

### [module_manager.h](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/module-manager/module_manager.h)
- L9 `<dirent.h>` 删;L10 `<sys/stat.h>` 删。

### [hal_ota.c](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/hal/hal_ota.c)
- L38 注释 "STM32 典型Flash页大小 2KB" 错误,改 1KB。
- MCU 构建不链接此 `.c`(由 `mcu/drivers/stm32f1_flash.c` 重新定义 `hal_ota_instance`)。

## 新增文件职责大纲

### `OTA-Update/hal/firmware_source.h`(核心解耦)
```c
typedef struct {
    uint32_t (*size)(void* ctx);
    int      (*read)(void* ctx, uint32_t off, void* buf, uint32_t len);
    void*    ctx;
} firmware_source_t;
```
所有"固件读取"经此抽象,PC 仿真用 file-backed、MCU 用 SPI Flash-backed,**业务逻辑零分支**。

### `mcu/drivers/stm32f1_flash.c`(Flash 擦写原语,无 hal_ota_t)
- `flash_unlock()`/`flash_lock()`:`FLASH->KEYR=0x45670123;0xCDEF89AB` 解锁/`FLASH->CR|=CR_LOCK` 上锁。
- `flash_erase_range(addr,size)`:按 1KB 页擦(`CR|=CR_PER; AR=page; CR|=CR_STRT; 等 BSY`)。
- `flash_write_word(addr,word)`:解锁→半字×2 编程(`CR|=PG`,写 `*(__IO uint16_t*)addr`)→等 BSY→上锁。
- `system_reset()`:`SCB->AIRCR = 0x05FA0004`(Cortex-M3 软复位)。
- **注**:`hal_ota_instance` 符号实由 `mcu/drivers/stm32f1_hal_ota.c` 导出(P4 桩,仅提供 `system_reset`,Flash 操作由 Bootloader 在 P6 用本文件的 `flash_*` 原语实现)。

### `mcu/drivers/stm32f1_hal_ota.c`(MCU hal_ota_t 实例)
- 导出 `hal_ota_t hal_ota_instance`(替 `OTA-Update/hal/hal_ota.c` 的桩)。
- App 阶段(P4)只提供 `system_reset()`,Flash 操作为空指针(App 不直接擦写,复位交 Bootloader)。
- Bootloader 不链接此文件,直接用 `stm32f1_flash.c` 原语。

### `mcu/drivers/w25qxx.c`(只读 + 升级标志写入)
- `w25qxx_read_id(mfr,type,cap)`:发 `9Fh` 读 JEDEC ID。
- `w25q_read(addr,buf,len)`:发 `03h+addr[3]`+连续读 `len`。
- `w25q_erase_range(addr,size)`:P7 加入,擦 SPI Flash 元数据扇区(写 upgrade_flag 前)。
- `w25q_write(addr,buf,len)`:P7 加入,页编程写元数据(boot_protocol.c 调用)。
- 固件镜像读取通过 `firmware_source_spiflash.c` 包装成 `firmware_source_t`。

### `mcu/drivers/lzma_alloc.c`(替 Alloc.c)
- `static uint8_t lzma_arena[LZMA_ARENA_SIZE]`(约 10KB);bump allocator(单次分配,不复用)。
- 导出 `const ISzAlloc g_McuAlloc`。
- `unzip_stream.h` 的 `g_FirmwareAlloc` 在 `TARGET_STM32` 下指向 `g_McuAlloc`。

### `mcu/printf_lite.c`(规避 newlib)
- 嵌入式轻量 printf(~1KB Flash),直接调 UART,无 `_sbrk`/malloc。
- MCU 编译 `-Dprintf=printf_lite` 使 `LOG_*` 走轻量 printf。

### `mcu/app/stm32f1xx_it.c`
- `SysTick_Handler`→`xPortSysTickHandler`、`PendSV_Handler`→`xPortPendSVHandler`、`SVC_Handler`→`vPortSVCHandler`(`#define` 别名)。
- 其余 IRQ 留空。

### `mcu/app/syscalls.c`
- `_write(fd,buf,len)`→UART 发送。
- `_sbrk`→指向静态 arena 或返回错误。
- `_close/_lseek/_fstat/_isatty`→-1/0。

### `mcu/cmsis/stm32f103xb.h`(自写最小化设备头)
- 不依赖完整 ARM CMSIS_5/ST CMSIS-Device(避免千 KB 头文件 + 大量未使用位)。
- 定义本项目用到的寄存器结构体与位掩码:`SCB_Type`(VTOR/AIRCR)、`SysTick_Type`、`NVIC_Type`、`RCC`、`FLASH`、`GPIOA/B`、`SPI1`、`USART1`、`AFIO`。
- 复位键常量:`SCB_AIRCR_VECTKEY`、`SCB_AIRCR_SYSRESETREQ_Msk`(供 `stm32f1_hal_ota.c` 的 `system_reset` 用)。
- Bootloader `jump_to_app` 设 `SCB->VTOR = APP_BASE_ADDR` 即用此头。

### `mcu/cmsis/system_stm32f1xx.c`(自写时钟初始化)
- `SystemInit(void)`:关 PLL→切 HSI→开 HSE 等就绪→`FLASH->ACR = PRFTBE | LATENCY_2`(72MHz 必须加 2 wait states)→配总线分频(HCLK=72/PCLK1=36/PCLK2=72)→PLL 源 HSE ×9=72MHz→等 PLLRDY→切 SYSCLK 到 PLL→`SystemCoreClock=72000000`。
- 早于 main,由启动文件 `Reset_Handler` 调用,不等 systick。
- App 与 Bootloader 共用同一份(`-I../cmsis`)。

### `mcu/bootloader/main.c`(无 RTOS)
流程(实际实现,见 [mcu/bootloader/main.c](file:///home/buduojun/Projects/Module/Bootloader-ota/mcu/bootloader/main.c)):
1. `SystemInit()`(由 startup 调)→`uart1_init()`→`spi1_init()`。
2. 打印 banner + App 区范围;`w25qxx_read_id()` 校验 SPI Flash(未检测到则直接跳 App)。
3. `boot_read_upgrade_flag()` 读 SPI Flash 元数据扇区。
4. 若 `flag == UPGRADE_FLAG_PENDING`,执行 `perform_upgrade()`:
   - `flash_unlock()`→`flash_erase_range(APP_BASE, APP_MAX_SIZE)`(48 页 × 1KB)→`flash_lock()`。
   - `flash_write_ctx_reset()` 重置写入上下文(4 字节 tail 缓冲,处理非对齐尾部)。
   - `perform_firmware_update_from_source(&g_spiflash_source, flash_output_write_cb, ...)`:流式 LZMA 解压 + 写内部 Flash。
   - `flash_write_ctx_flush_tail()` 刷出最后不足 4 字节。
   - 校验 App 起始 MSP 非 0xFFFFFFFF/0x00000000。
   - `boot_write_upgrade_flag(UPGRADE_FLAG_DONE)` 清标志(失败写 `UPGRADE_FLAG_ERROR`)。
5. 跳转 App @0x08004000:`jump_to_app()` 设 `SCB->VTOR` + MSP + 跳到 App reset handler。
- 不创建任何 FreeRTOS 对象,`main` 裸跑。

### `mcu/bootloader/boot_protocol.{c,h}`(P7 新增)
- 定义 `UPGRADE_FLAG_NONE/PENDING/DONE/ERROR` 四态枚举。
- `boot_read_upgrade_flag()`/`boot_write_upgrade_flag(flag)`:读写 SPI Flash 元数据区 `0x080000` 的 4 字节标志。
- 写前自动擦除元数据扇区(`w25q_erase_range`)+ 页编程(`w25q_write`)。
- App 在 `action_start_updating` 调 `boot_write_upgrade_flag(PENDING)`+`system_reset()`;Bootloader 复位后检测此标志进入升级。

### `mcu/startup_stm32f103xb.s`(vendor,改 ORIGIN/向量)
- `.isr_vector` 表(初始 SP = `_estack`;Reset/NMI/HardFault/...;`SysTick/PendSV/SVC` 指向 FreeRTOS port 弱符号)。
- `Reset_Handler`:复制 `.data` `.bss`,调 `SystemInit`、`__libc_init_array`、`main`,死循环。

### `mcu/STM32F103C8Tx_FLASH.ld`(App/Bootloader 共用,参数化 ORIGIN/LENGTH)
单一脚本服务两个目标,通过链接器 `--defsym` 区分 ORIGIN/LENGTH,无需维护两份高度重复的 .ld:

```ld
/* 默认值:App @ 0x08004000, 48KB
 * Boot 构建在 LDFLAGS 用 -Wl,--defsym=FLASH_ORIGIN=0x08000000 等覆盖 */
PROVIDE(FLASH_ORIGIN = 0x08004000);
PROVIDE(FLASH_LENGTH = 48K);

MEMORY {
    FLASH (rx)  : ORIGIN = FLASH_ORIGIN, LENGTH = FLASH_LENGTH
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
/* 其余 .isr_vector/.text/.data/.bss 段与单脚本版完全相同 */
```

**机制**:`PROVIDE(symbol = value)` 仅在该符号未定义时赋值;GNU ld 的 `--defsym` 在脚本解析期生效且优先级高于 `PROVIDE`,故 Boot 构建传命令行值,App 构建回退默认值。MEMORY 区域的 ORIGIN/LENGTH 接受表达式,可在脚本解析期被符号求值。

**Makefile 调用**:
- App(用默认值,无需额外参数):
  ```make
  LDSCRIPT = ../STM32F103C8Tx_FLASH.ld
  LDFLAGS  = $(CPUFLAGS) -T$(LDSCRIPT) -Wl,--gc-sections ...
  ```
- Bootloader(命令行覆盖):
  ```make
  LDSCRIPT = ../STM32F103C8Tx_FLASH.ld
  LDFLAGS  = $(CPUFLAGS) -T$(LDSCRIPT) \
             -Wl,--defsym,FLASH_ORIGIN=0x08000000 \
             -Wl,--defsym,FLASH_LENGTH=0x4000 \
             -Wl,--gc-sections ...
  ```

**实测验证**:`make -C mcu/bootloader` 输出 `text=14020 data=20 bss=8520`,与合并前(双 .ld 版)完全一致;`make -C mcu/app` 同样通过,链接地址 0x08004000 正确(App 默认值生效)。

**为何不用两份 .ld**:两份脚本除 `ORIGIN/LENGTH` 与首行注释外其余 100+ 行完全重复,改 SECTIONS 布局要同步两处,易漏改。`--defsym` 方案把"差异点"集中到 Makefile 一行参数,SECTIONS 维护只动一份。

## 文件来源(vendor vs 自写)

| 件 | 来源 | 目标 |
|---|---|---|
| ARM_CM3 port.c/portmacro.h | vendor:FreeRTOS/FreeRTOS-Kernel `portable/GCC/ARM_CM3/` | `mcu/freertos_port/` |
| stm32f103xb.h | **自写最小化**(不依赖完整 CMSIS-Device,仅定义本项目用到的外设/寄存器位) | `mcu/cmsis/` |
| system_stm32f1xx.c | **自写**(HSE×PLL9=72MHz,2 WS,~1.5KB) | `mcu/cmsis/` |
| startup_stm32f103xb.s | vendor:ST Cube `gcc/`(改向量指向 port) | `mcu/` |
| STM32F103C8Tx_FLASH.ld | vendor:ST Cube `gcc/` 起手,自改为 **App/Boot 共用单脚本**:`PROVIDE(FLASH_ORIGIN/LENGTH)` 默认 App 值,Boot 用 `--defsym` 覆盖 | `mcu/` |

> 实际实现阶段决定不引入完整 CMSIS_5/ST CMSIS-Device(数千 KB 头文件 + 大量未使用位),改写最小自包含版本,节省编译时间且 `--gc-sections` 后 Bootloader 体积可控。其他 vendor 文件可由用户从 STM32CubeFx 包或 GitHub 仓库放入。

## Makefile 拆分

- **根 Makefile**(PC 仿真)保留不动:`-pthread -DUSE_FREERTOS`,链 posix port,libc printf。
- **`mcu/app/Makefile`**(App,实际见 [mcu/app/Makefile](file:///home/buduojun/Projects/Module/Bootloader-ota/mcu/app/Makefile)):`arm-none-eabi-gcc`,`-mthumb -mcpu=cortex-m3 -mfloat-abi=soft -Os -ffunction-sections -fdata-sections`,链 STM32F103C8Tx_FLASH.ld;`-DTARGET_STM32 -DLOG_LEVEL=3 -DLOG_LOC_ENABLE=0 -DINPUT_BUFFER_SIZE=512 -DOUTPUT_BUFFER_SIZE=512`;`-nostartfiles --specs=nosys.specs`;`-Wl,--gc-sections`;排除 posix/port.c、unzip.c、zip.c、Alloc.c、timers.c、hal_ota.c(由 `mcu/drivers/stm32f1_hal_ota.c` 替,提供 `hal_ota_instance`);新增 `mcu/drivers/boot_protocol.c`(读写 upgrade_flag)。源:FreeRTOS(list/queue/tasks/event_groups/heap_4 + ARM_CM3 port)+ stm32f1_hal_ota + stm32f1_flash + stm32f1_gpio/spi/uart + w25qxx + firmware_source_spiflash + stm32f1xx_it + syscalls + printf_lite + OTA-Update 共享(ota_main/ota_fsm/md5/module_manager)。输出 `build/app.elf`→`app.bin`。
- **`mcu/bootloader/Makefile`**(Bootloader,实际见 [mcu/bootloader/Makefile](file:///home/buduojun/Projects/Module/Bootloader-ota/mcu/bootloader/Makefile)):同 CPU/标志但**不带 `-DUSE_FREERTOS`**,不编 FreeRTOS/ota_main/fsm;**与 App 共用 `STM32F103C8Tx_FLASH.ld`**,通过 `LDFLAGS` 加 `-Wl,--defsym,FLASH_ORIGIN=0x08000000 -Wl,--defsym,FLASH_LENGTH=0x4000` 把链接基址从默认 App(0x08004000/48K)覆盖为 Boot(0x08000000/16K)。源:main.c、boot_protocol.c、stm32f1_flash.c、stm32f1_spi.c、w25qxx.c、stm32f1_uart.c、stm32f1_gpio.c、firmware_source_spiflash.c、unzip_stream.c、LzmaDec.c、7zCrc.c、7zCrcOpt.c、lzma_alloc.c、stm32f1xx_it.c、syscalls.c、printf_lite、startup_stm32f103xb.s、system_stm32f1xx.c。输出 `build/boot.elf`→`boot.bin`。

## 关键 trade-off 与风险

1. **LZMA 默认 props 致 RAM 不可用** — 必须主机端重打包 lc=0/lp=0/pb=0/dict=4KB。修改 [zip.c](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/lzma/zip/zip.c) `ZIP_LZMA_DICT_SIZE/LC/LP/PB`(L29-33)加 `#ifdef MCU_DICT` 分支。probs 降至 ~5.4KB。**风险高,必做**。
2. **`unzip_stream.c` 栈缓冲 8KB** — 移 static + 缩 512B。**风险高**。
3. **`Alloc.c` 不能上 MCU** — 用 `lzma_alloc.c` 替,MCU 构建排除 Alloc.c。**风险中**。
4. **App 原地自升级断电不安全** — 由 App 只置标志+复位,Bootloader 内做擦写,元数据在 SPI Flash,断电可重试。**风险低(已规避)**。
5. **Bootloader 16KB 与 LZMA SDK ~9KB 极紧** — 后备扩到 20KB(App 压到 44KB)。**风险中**。
6. **newlib printf 重 Flash/RAM** — 用 `printf_lite` 规避。**风险中**。
7. **`ota_main.h` 内 `static` 全局** — 单 TU 包含可;若 MCU 拆 TU 需重构为 extern。**风险低**。
8. **F103C8 页大小 1KB** — 桩代码注释错为 2KB(`hal_ota.c:38`),擦写驱动按 1KB。**风险低**。
9. **中断优先级** — `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` 必设。**风险中**。
10. **链接脚本单/双方案权衡** — 单脚本 + `--defsym`(已采用):维护 1 份,差异点集中在 Makefile 一行,改 SECTIONS 不会漏改另一份;代价是 MEMORY 区域的 ORIGIN/LENGTH 不能用字面量校验(需依赖符号求值),`--defsym` 拼写错只会运行时崩(链接器不报"区域不存在")。双脚本:直观,可读性好,每份独立校验;代价是 100+ 行重复,改一处要同步两处。本项目选单脚本 + 在 Makefile 注释明确写出默认值与覆盖值,并通过 `arm-none-eabi-size`/`objdump` 验证 text 段基址。**风险低(已实施)**。

## 分阶段验证计划

| 阶段 | 目标 | 验证手段 | 状态 |
|---|---|---|---|
| P0 工具链 | `arm-none-eabi-gcc` 可用,空工程链接 | 烧录 blinky(LED PA5 翻转),复位可见 | ✅ 完成 |
| P1 内核 blinky | FreeRTOS ARM_CM3 port + 1 任务 LED 翻转 | LED 以 1Hz 闪(验证 SysTick/PendSV/SVC/vTaskDelay) | ✅ 完成 |
| P2 UART printf | `printf_lite`→USART1,`LOG_INFO` 输出 | 串口见 banner,多任务 printf 串行化无乱 | ✅ 完成 |
| P3 SPI Flash | `w25q_read_id()` + 读头 | 串口打印 W25Q ID 与 FirmwareHeader magic/size | ✅ 完成 |
| P4 OTA 任务 | ota_task + download_task(读 SPI Flash 元数据) | 状态机走到 DOWNLOADING→VERIFYING,MD5(buffer)通过 | ✅ 完成 |
| P5 LZMA 解码(App 内,可选) | `perform_firmware_update_stream_from_source` 解 .lzma 到 RAM | 打印解压 size + CRC 通过(小固件,仅验证算法) | ✅ 完成 |
| P6 Bootloader 升级 | Bootloader 从 SPI Flash 解 .lzma 写 App 区 + 跳转 | 复位后 App 版本字串更新;断电重试通过 | ✅ 完成(App text=16196, Boot text=14020) |
| P7 端到端 | App OTA 任务置标志→复位→Boot 升级→新 App 起来 | 版本 V1.0→V2.0 切换成功;断电重试通过(标志未清前 PENDING 持续) | ✅ 完成 |
| P8 回归 | PC 仿真仍 `make run` / `make regression` 通过 | Summary Success:5 Failed:0(基线,`make regression` target 已加入根 Makefile,`stdbuf -o0` + `timeout 90s` 处理任务驻留) | 🔄 进行中 |

## 实施顺序

1. 建 `mcu/` 骨架 + 工具链 + 启动 + **单一链接脚本 `STM32F103C8Tx_FLASH.ld`(`PROVIDE(FLASH_ORIGIN/LENGTH)` 默认 App 值,Boot 用 `--defsym` 覆盖)** + `system_stm32f1xx.c` + LED(P0)
2. 引 ARM_CM3 port + `FreeRTOSConfig.h`(MCU)+ 1 任务(P1)
3. UART + `printf_lite`(P2)
4. `firmware_source.h` 抽象 + PC file-backed 实现(先在 PC 仿真切到抽象,回归通过)
5. 改 `unzip_stream.c`/`firmware_update.c`/`ota_fsm.c`/`md5.c` 走 `firmware_source_t`(条件编译保 PC)
6. SPI Flash + W25Q 驱动 + source_spiflash 实现(P3)
7. OTA 任务 + MD5 buffer 校验(P4)
8. 主机端 `zip.c` 加小字典 build flag,重建 `firmware_mcu.bin.lzma`
9. `lzma_alloc.c` + (可选)App 内小固件解码验证(P5)
10. Bootloader main + LZMA 解码 + 写 App + 跳转(P6)
11. 端到端 + 断电重试(P7)
12. PC 仿真回归(P8)

## 关键修改文件清单

- [unzip_stream.c](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/lzma/flow-unzip/unzip_stream.c) — LZMA 解码主路径,去 fopen/fread、栈缓冲移 static、切 firmware_source
- [firmware_update.c](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/update-table/firmware_update.c) — 分区表重定义 + 去文件依赖 + 缩 buf
- [ota_fsm.c](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/fsm/ota_fsm.c) — 状态机动作改造
- [FreeRTOSConfig.h](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/freertos/FreeRTOSConfig.h) — RTOS 配置(MCU 副本衍生)
- [ota_main.c](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/app/ota_main.c) — 任务栈深/钩子实现
- [LzmaDec.c](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/lzma/sdk/LzmaDec.c) — probs 大小公式验证依据(不改,但决定 lc=0 重打包)
- [zip.c](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/lzma/zip/zip.c) — 主机端小字典 build flag
- [md5.c](file:///home/buduojun/Projects/Module/Bootloader-ota/OTA-Update/module/md5/md5.c) — 增加 buffer/source MD5
- [STM32F103C8Tx_FLASH.ld](file:///home/buduojun/Projects/Module/Bootloader-ota/mcu/STM32F103C8Tx_FLASH.ld) — App/Boot 共用单脚本(原计划两份),`PROVIDE(FLASH_ORIGIN/LENGTH)` 默认 App 值,Boot 经 `--defsym` 覆盖;对应 [mcu/bootloader/Makefile](file:///home/buduojun/Projects/Module/Bootloader-ota/mcu/bootloader/Makefile#L26-32) LDFLAGS 加 `--defsym,FLASH_ORIGIN=0x08000000 --defsym,FLASH_LENGTH=0x4000`

## 范围声明

本规划涉及约 30+ 新文件(其中 vendor 6 个 + 自写 20+)与 10+ 文件修改,需分阶段执行。若执行阶段发现具体实现有更优方案(例如 vendor 文件下载受限、STM32 HAL 库引入成本、`ota_main.h` 静态全局重构等),会即时与你对齐再继续,不会擅自变更大方向。
