# platforms/stm32f103/bootloader/Makefile —— Bootloader 固件(STM32F103C8T6,16KB @ 0x08000000)
# 无 FreeRTOS,无 HOST_SIM;裸机 + W25Q + 内部 Flash 擦写 + LZMA 解压
# 工具链
CC       = arm-none-eabi-gcc
OBJCOPY  = arm-none-eabi-objcopy
SIZE     = arm-none-eabi-size

# CPU 与编译选项
CPUFLAGS = -mthumb -mcpu=cortex-m3 -mfloat-abi=soft
CFLAGS  = $(CPUFLAGS) -Os -g -ffunction-sections -fdata-sections \
          -Wall -Wextra -std=c11
CFLAGS += -DTARGET_STM32 -DLOG_LEVEL=3 -DLOG_LOC_ENABLE=0
CFLAGS += -DINPUT_BUFFER_SIZE=512 -DOUTPUT_BUFFER_SIZE=512
# CRC32 用 1 表软件实现(默认 12 表占 12KB RAM,1 表只占 1KB;Bootloader 不在乎速度)
CFLAGS += -DZ7_CRC_NUM_TABLES=1
# 注:不加 -DHOST_SIM —— 走 MCU 路径(unzip_stream 用 g_McuAlloc + firmware_source_t)

# 头文件搜索路径
#   平台本地: .. / ../cmsis / ../drivers / .
#   平台无关库: ../../../lib / ../../../lib/app / ../../../lib/hal / ../../../lib/lzma/sdk / ../../../lib/lzma/flow-unzip
CPPFLAGS = -I.. -I../cmsis -I../drivers -I. \
           -I../../../lib \
           -I../../../lib/app -I../../../lib/hal \
           -I../../../lib/lzma/sdk \
           -I../../../lib/lzma/flow-unzip

# 链接:与 App 共用 STM32F103C8Tx_FLASH.ld,通过 --defsym 覆盖 ORIGIN/LENGTH
LDSCRIPT = ../STM32F103C8Tx_FLASH.ld
LDFLAGS  = $(CPUFLAGS) -T$(LDSCRIPT) \
           -Wl,--defsym,FLASH_ORIGIN=0x08000000 \
           -Wl,--defsym,FLASH_LENGTH=0x4000 \
           -Wl,--gc-sections -Wl,-Map=$(BUILD)/boot.map \
           --specs=nosys.specs -nostartfiles

BUILD = build

# Bootloader 源文件:
#   - startup + 系统时钟
#   - GPIO/UART/SPI/W25Q
#   - firmware_source_spiflash(SPI Flash 固件源)
#   - stm32f1_flash(内部 Flash 擦写)
#   - lzma_alloc(静态 arena 分配器,替 Alloc.c)
#   - unzip_stream(LZMA 流式解压 MCU 路径)
#   - LzmaDec + 7zCrc + 7zCrcOpt(LZMA SDK,不含 Alloc.c/7zAlloc.c/LzmaEnc.c)
#   - printf_lite + syscalls(_write 重定向 + _init/_fini stubs)
#   - boot_protocol(upgrade_flag 读写)
#   - main(Bootloader 入口)
SRCS = ../startup_stm32f103xb.s \
       ../cmsis/system_stm32f1xx.c \
       ../drivers/stm32f1_gpio.c \
       ../drivers/stm32f1_uart.c \
       ../drivers/stm32f1_spi.c \
       ../drivers/w25qxx.c \
       ../drivers/firmware_source_spiflash.c \
       ../drivers/stm32f1_flash.c \
       ../drivers/lzma_alloc.c \
       ../printf_lite.c \
       ../app/syscalls.c \
       boot_protocol.c \
       main.c \
       ../../../lib/lzma/flow-unzip/unzip_stream.c \
       ../../../lib/lzma/sdk/LzmaDec.c \
       ../../../lib/lzma/sdk/7zCrc.c \
       ../../../lib/lzma/sdk/7zCrcOpt.c

OBJS = $(addprefix $(BUILD)/, $(notdir $(SRCS:.c=.o)))
OBJS := $(OBJS:.s=.o)
DEPS = $(OBJS:.o=.d)

# 用 vpath 指令(只搜对应扩展名的依赖),避免 VPATH 变量误把
# ../app/build/*.o 当作目标(那会导致 make 认为 .o 已是最新)
vpath %.c ..:../cmsis:../drivers:../app:.:\
        ../../../lib/lzma/flow-unzip:\
        ../../../lib/lzma/sdk
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
