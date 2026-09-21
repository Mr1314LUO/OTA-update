# ==========================================
# OTA-update 项目 Makefile
# ==========================================

# ---------- 工具链 ----------
# 本地编译使用 gcc；交叉编译可改为:
#   CC = arm-none-eabi-gcc
CC      ?= gcc

# ---------- 编译选项 ----------
# 通用编译选项: 开启警告、C11 标准、调试信息
CFLAGS  ?= -Wall -Wextra -std=c11 -g
# 头文件搜索路径:
#   .                          —— 顶层模块路径式 include（如 hal/hal_ota.h、printf.h）
#   module                     —— 模块内部路径式 include（如 fsm/、lzma/、md5/ 等）
#   module/lzma/sdk            —— LZMA SDK 头文件（LzmaDec.h 等）
CPPFLAGS += -I. \
                -Iapp \
                -Imodule \
                -Imodule/lzma/sdk \
                -D_POSIX_C_SOURCE=200809L \
                -D_DEFAULT_SOURCE

# ---------- 目标、源文件与构建目录 ----------
TARGET    := ota_main
BUILD_DIR := build

# 使用当前仓库真实目录结构：项目根目录就是 ota-Update/
SRCS := app/ota_main.c \
        hal/hal_ota.c \
        module/fsm/ota_fsm.c \
        module/update-table/firmware_update.c \
        module/module-manager/module_manager.c \
        module/md5/md5.c \
        module/lzma/unzip/unzip.c \
        module/lzma/zip/zip.c \
        module/lzma/flow-unzip/unzip_stream.c \
        module/lzma/sdk/Alloc.c \
        module/lzma/sdk/LzmaDec.c \
        module/lzma/sdk/7zCrc.c \
        module/lzma/sdk/7zCrcOpt.c

# 目标文件统一输出到 BUILD_DIR，并保留源码目录层级
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))
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

# 伪目标声明
.PHONY: all clean run

# 引入自动生成的依赖文件
-include $(DEPS)