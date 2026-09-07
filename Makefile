# ==========================================
# OTA-update 项目 Makefile
# ==========================================

# ---------- 工具链 ----------
# 本地编译使用 gcc；交叉编译可改为:
#   CC = arm-none-eabi-gcc
CC      ?= gcc

# ---------- 编译选项 ----------
# 通用编译选项: 开启警告、C11 标准、调试信息
# -pthread: FreeRTOS POSIX 移植层需要（编译与链接均需携带）
CFLAGS  ?= -Wall -Wextra -std=c11 -g
CFLAGS  += -pthread
# 头文件搜索路径:
#   OTA-Update                  —— 顶层模块路径式 include（如 hal/hal_ota.h）
#   OTA-Update/app              —— 应用层头文件（如 printf.h、ota_main.h）
#   OTA-Update/module           —— 模块内部路径式 include（如 fsm/、lzma/、md5/ 等）
#   OTA-Update/module/lzma/sdk  —— LZMA SDK 头文件（LzmaDec.h 等）
#   OTA-Update/module/freertos  —— FreeRTOSConfig.h 及内核头文件/移植层
FREERTOS_DIR := OTA-Update/module/freertos
CPPFLAGS += -IOTA-Update \
                -IOTA-Update/app \
                -IOTA-Update/hal \
                -IOTA-Update/module \
                -IOTA-Update/module/lzma/sdk \
                -I$(FREERTOS_DIR) \
                -I$(FREERTOS_DIR)/include \
                -I$(FREERTOS_DIR)/posix \
                -D_POSIX_C_SOURCE=200809L \
                -D_DEFAULT_SOURCE \
                -DUSE_FREERTOS \
                -DHOST_SIM

# ---------- 目标、源文件与构建目录 ----------
TARGET    := ota_main
BUILD_DIR := build

SRCS := OTA-Update/app/ota_main.c \
        OTA-Update/hal/hal_ota.c \
        OTA-Update/module/fsm/ota_fsm.c \
        OTA-Update/module/update-table/firmware_update.c \
        OTA-Update/module/module-manager/module_manager.c \
        OTA-Update/module/md5/md5.c \
        OTA-Update/module/lzma/unzip/unzip.c \
        OTA-Update/module/lzma/zip/zip.c \
        OTA-Update/module/lzma/flow-unzip/unzip_stream.c \
        OTA-Update/module/lzma/sdk/Alloc.c \
        OTA-Update/module/lzma/sdk/LzmaDec.c \
        OTA-Update/module/lzma/sdk/7zCrc.c \
        OTA-Update/module/lzma/sdk/7zCrcOpt.c \
        OTA-Update/module/freertos/list.c \
        OTA-Update/module/freertos/queue.c \
        OTA-Update/module/freertos/tasks.c \
        OTA-Update/module/freertos/timers.c \
        OTA-Update/module/freertos/event_groups.c \
        OTA-Update/module/freertos/heap_4.c \
        OTA-Update/module/freertos/posix/port.c \
        OTA-Update/module/freertos/posix/utils/wait_for_event.c

# 在子目录中搜索源文件，使 $(BUILD_DIR)/xxx.o 能匹配到对应源文件
VPATH := OTA-Update/app:OTA-Update/hal: \
        OTA-Update/module/fsm: \
        OTA-Update/module/update-table: \
        OTA-Update/module/module-manager: \
        OTA-Update/module/md5: \
        OTA-Update/module/lzma/unzip: \
        OTA-Update/module/lzma/zip: \
        OTA-Update/module/lzma/flow-unzip: \
        OTA-Update/module/lzma/sdk: \
        $(FREERTOS_DIR): \
        $(FREERTOS_DIR)/posix: \
        $(FREERTOS_DIR)/posix/utils

# 目标文件统一输出到 BUILD_DIR，依赖文件(.d)由 -MMD 自动生成
OBJS := $(addprefix $(BUILD_DIR)/, $(notdir $(SRCS:.c=.o)))
DEPS := $(OBJS:.o=.d)

# ---------- 规则 ----------
# 默认目标：仅完成构建
all: $(BUILD_DIR)/$(TARGET)
	./$(BUILD_DIR)/$(TARGET)

# 链接（unzip.c 依赖系统 liblzma）
$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -llzma

# 编译（模式规则）+ 自动依赖生成（-MMD -MP）
# 在 recipe 中确保输出目录存在（order-only 前提在 -include .d 时不可靠）
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# 自动创建构建目录
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 运行程序
run: $(BUILD_DIR)/$(TARGET)
	./$(BUILD_DIR)/$(TARGET)

# 清理编译产物
clean:
	rm -rf $(BUILD_DIR)

# P8: PC 仿真回归测试
# 程序到达 SUCCESS/FAILED 终态后任务驻留(while(1)),需 timeout 杀掉;
# 通过条件:Summary 中 Failed:0 且出现 "升级成功"(到达 SUCCESS 终态)
# 终态输出点已加 fflush(stdout),无需 stdbuf -o0; 90s 余量足够(实测 ~35s)
regression: $(BUILD_DIR)/$(TARGET)
	@echo "=== P8: PC 仿真回归 ==="
	@rm -f /tmp/ota_p8.log
	@timeout 90s ./$(BUILD_DIR)/$(TARGET) > /tmp/ota_p8.log 2>&1; \
	rc=$$?; \
	if [ $$rc -eq 124 ]; then echo "(超时 90s 终止 — 正常:终态后驻留)"; \
	else echo "(进程退出 rc=$$rc)"; fi; \
	echo ""; echo "=== Summary ==="; \
	grep -E 'Success:|Failed:|Skipped:|升级成功|升级失败' /tmp/ota_p8.log || true; \
	echo ""; echo "=== 判定 ==="; \
	if grep -q 'Failed:  0' /tmp/ota_p8.log && grep -q '升级成功' /tmp/ota_p8.log; then \
		echo "P8 PASS: Summary Failed:0, 已到达 SUCCESS"; exit 0; \
	else \
		echo "P8 FAIL: 见 /tmp/ota_p8.log, 尾部如下:"; \
		tail -n 30 /tmp/ota_p8.log; exit 1; \
	fi

# 伪目标声明
.PHONY: all clean run regression

# 引入自动生成的依赖文件
-include $(DEPS)