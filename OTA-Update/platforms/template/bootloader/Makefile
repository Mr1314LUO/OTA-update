# bootloader/Makefile —— Bootloader 固件构建（模板，裸机无 RTOS）
# TODO: 复制到 platforms/<chip>/bootloader/ 后修改 CC / CPUFLAGS / 链接脚本 / 源文件列表
# 参考：platforms/stm32f103/bootloader/Makefile

# ---------- 工具链（TODO: 按芯片交叉编译器修改） ----------
CC       = arm-none-eabi-gcc
OBJCOPY  = arm-none-eabi-objcopy
SIZE     = arm-none-eabi-size

# ---------- CPU 选项（TODO: 按架构修改） ----------
CPUFLAGS = -mthumb -mcpu=cortex-m3 -mfloat-abi=soft
CFLAGS  = $(CPUFLAGS) -Os -g -ffunction-sections -fdata-sections \
          -Wall -Wextra -std=c11
CFLAGS += -DTARGET_STM32 -DLOG_LEVEL=3 -DLOG_LOC_ENABLE=0
CFLAGS += -DINPUT_BUFFER_SIZE=512 -DOUTPUT_BUFFER_SIZE=512
# CRC32 用 1 表软件实现省 RAM（默认 12 表占 12KB RAM，1 表仅 1KB）
CFLAGS += -DZ7_CRC_NUM_TABLES=1
# 注：不加 -DHOST_SIM —— 走 MCU 解压路径（g_McuAlloc + firmware_source_t）

# ---------- 头文件搜索路径 ----------
CPPFLAGS = -I.. -I../drivers -I. \
           -I../../../lib -I../../../lib/app -I../../../lib/hal \
           -I../../../lib/lzma/sdk -I../../../lib/lzma/flow-unzip

# ---------- 链接（TODO: 链接脚本改为本芯片的 .ld；
#          Bootloader 通过 --defsym 覆盖 Flash 起始/长度，与 App 共用同一 .ld） ----------
LDSCRIPT = ../chip_link.ld
LDFLAGS  = $(CPUFLAGS) -T$(LDSCRIPT) \
           -Wl,--defsym,FLASH_ORIGIN=0x08000000 \
           -Wl,--defsym,FLASH_LENGTH=0x4000 \
           -Wl,--gc-sections -Wl,-Map=$(BUILD)/boot.map \
           --specs=nosys.specs -nostartfiles

BUILD = build

# ---------- 源文件 ----------
# TODO: 替换为芯片 startup/CMSIS/syscalls
SRCS = main.c \
       boot_protocol.c \
       ../printf_lite.c \
       ../drivers/board_uart.c \
       ../drivers/board_storage.c \
       ../drivers/board_flash.c \
       ../drivers/firmware_source_impl.c \
       ../drivers/lzma_alloc.c \
       ../../../lib/lzma/flow-unzip/unzip_stream.c \
       ../../../lib/lzma/sdk/LzmaDec.c \
       ../../../lib/lzma/sdk/7zCrc.c \
       ../../../lib/lzma/sdk/7zCrcOpt.c
# 注意：不含 Alloc.c（用 lzma_alloc.c 替代）、不含 FreeRTOS

OBJS = $(addprefix $(BUILD)/, $(notdir $(SRCS:.c=.o)))
OBJS := $(OBJS:.s=.o)
DEPS = $(OBJS:.o=.d)

# vpath：只搜对应扩展名
vpath %.c .. ../drivers .
vpath %.c ../../../lib/lzma/flow-unzip ../../../lib/lzma/sdk
vpath %.s ..

TARGET = boot

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
