# platforms/stm32f103/app/Makefile —— App 固件(STM32F103C8T6)
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

# 头文件搜索路径
#   平台本地: .. / ../cmsis / ../drivers / ../freertos_port / . / ../bootloader
#   平台无关库: ../../../lib / ../../../lib/app / ../../../lib/hal / ../../../lib/lzma/sdk
#   RTOS 内核: ../../../os/freertos / ../../../os/freertos/include
CPPFLAGS = -I.. -I../cmsis -I../drivers -I../freertos_port -I. \
           -I../bootloader \
           -I../../../lib \
           -I../../../lib/app -I../../../lib/hal \
           -I../../../lib/lzma/sdk \
           -I../../../os/freertos -I../../../os/freertos/include

# 链接
LDSCRIPT = ../STM32F103C8Tx_FLASH.ld
LDFLAGS  = $(CPUFLAGS) -T$(LDSCRIPT) -Wl,--gc-sections -Wl,-Map=$(BUILD)/app.map \
           --specs=nosys.specs -nostartfiles

BUILD = build

# P1 加入 FreeRTOS ARM_CM3 port + 内核源(不含 Timers, 因 configUSE_TIMERS=0)
# P4 加入 OTA 共享源:ota_fsm.c / md5.c / module_manager.c(注:ota_main.c 由 main.c 调用)
# P7 加入 boot_protocol.c(App 需读写 upgrade_flag)
SRCS = ../cmsis/system_stm32f1xx.c \
       ../drivers/stm32f1_gpio.c \
       ../drivers/stm32f1_uart.c \
       ../drivers/stm32f1_spi.c \
       ../drivers/w25qxx.c \
       ../drivers/firmware_source_spiflash.c \
       ../drivers/stm32f1_hal_ota.c \
       ../bootloader/boot_protocol.c \
       ../printf_lite.c \
       main.c \
       stm32f1xx_it.c \
       syscalls.c \
       ../startup_stm32f103xb.s \
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

OBJS = $(addprefix $(BUILD)/, $(notdir $(SRCS:.c=.o)))
OBJS := $(OBJS:.s=.o)
DEPS = $(OBJS:.o=.d)

# vpath 指令(非 VPATH 变量):仅搜索 .c/.s 源文件
# 避免误命中 ../bootloader/build/*.o 等 .o 文件导致链接到错误对象
vpath %.c ../cmsis ../drivers .. . ../freertos_port ../bootloader
vpath %.c ../../../os/freertos ../../../lib/app
vpath %.c ../../../lib/fsm ../../../lib/md5
vpath %.c ../../../lib/module-manager
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
