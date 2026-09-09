# app/Makefile —— App 固件构建（模板）
# TODO: 复制到 platforms/<chip>/app/ 后修改 CC / CPUFLAGS / 启动文件 / 源文件列表
# 参考：platforms/stm32f103/app/Makefile

# ---------- 工具链（TODO: 按芯片交叉编译器修改） ----------
CC       = arm-none-eabi-gcc
OBJCOPY  = arm-none-eabi-objcopy
SIZE     = arm-none-eabi-size

# ---------- CPU 选项（TODO: 按架构修改 -mcpu/-mthumb 等） ----------
CPUFLAGS = -mthumb -mcpu=cortex-m3 -mfloat-abi=soft
CFLAGS  = $(CPUFLAGS) -Os -g -ffunction-sections -fdata-sections \
          -Wall -Wextra -std=c11
CFLAGS += -DTARGET_STM32 -DLOG_LEVEL=3 -DLOG_LOC_ENABLE=0
CFLAGS += -DINPUT_BUFFER_SIZE=512 -DOUTPUT_BUFFER_SIZE=512
# 注意：MCU 路径不加 -DHOST_SIM（走 firmware_source + printf_lite 路径）

# ---------- 头文件搜索路径 ----------
# 平台本地：..（printf_lite.h/启动/链接脚本）/ drivers / freertos_port / . / bootloader
# 平台无关：../../../lib（+app/hal/lzma）、../../../os/freertos（内核）
CPPFLAGS = -I.. -I../drivers -I../freertos_port -I. -I../bootloader \
           -I../../../lib -I../../../lib/app -I../../../lib/hal \
           -I../../../lib/lzma/sdk \
           -I../../../os/freertos -I../../../os/freertos/include

# ---------- 链接（TODO: 链接脚本改为本芯片的 .ld） ----------
LDSCRIPT = ../chip_link.ld
LDFLAGS  = $(CPUFLAGS) -T$(LDSCRIPT) -Wl,--gc-sections -Wl,-Map=$(BUILD)/app.map \
           --specs=nosys.specs -nostartfiles

BUILD = build

# ---------- 源文件 ----------
# TODO: 替换为芯片的 startup/CMSIS/中断/syscalls 与 FreeRTOS port
SRCS = main.c \
       ../printf_lite.c \
       ../drivers/board_uart.c \
       ../drivers/board_storage.c \
       ../drivers/hal_ota_impl.c \
       ../drivers/firmware_source_impl.c \
       ../drivers/lzma_alloc.c \
       ../bootloader/boot_protocol.c \
       ../freertos_port/port.c \
       ../../../os/freertos/list.c \
       ../../../os/freertos/queue.c \
       ../../../os/freertos/tasks.c \
       ../../../os/freertos/event_groups.c \
       ../../../os/freertos/heap_4.c \
       ../../../lib/app/ota_main.c \
       ../../../lib/fsm/ota_fsm.c \
       ../../../lib/md5/md5.c \
       ../../../lib/module-manager/module_manager.c
# TODO: 芯片专属：../startup_<chip>.s / ../cmsis/system_<chip>.c / syscalls.c / <it>.c

OBJS = $(addprefix $(BUILD)/, $(notdir $(SRCS:.c=.o)))
OBJS := $(OBJS:.s=.o)
DEPS = $(OBJS:.o=.d)

# vpath：只搜对应扩展名，避免误把其它 build/*.o 当目标
vpath %.c .. ../drivers ../freertos_port . ../bootloader
vpath %.c ../../../os/freertos ../../../lib/app
vpath %.c ../../../lib/fsm ../../../lib/md5 ../../../lib/module-manager
vpath %.s ..

TARGET = app

all: $(BUILD)/$(TARGET).elf

$(BUILD)/$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	@mkdir -p $(BUILD)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) -lc
	$(SIZE) $@
	$(OBJCOPY) -O binary $@ $(BUILD)/$(TARGET).bin

$(BUILD)/%.o: %.c
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -x assembler-with-cpp -c $< -o $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
-include $(DEPS)
