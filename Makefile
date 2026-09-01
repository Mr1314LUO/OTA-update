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
# 预处理器: 头文件搜索路径（各子目录头文件）
CPPFLAGS += -I. -Ifsm-table-driven -IHAL-ota -Iflashing-firmware -Imd5_jiao-yan -Ilzma -Ilzma/unzip -Ilzma/zip -Ilzma/flow-unzip -I../lzma2602/C -D_POSIX_C_SOURCE=200809L

# ---------- 目标、源文件与构建目录 ----------
TARGET    := ota_main
BUILD_DIR := build

# 源文件（按实际目录分布）；ota_engine 的函数分别实现在
# 根目录 ota_main.c 与 fsm-table-driven/table_driven_fsm.c 中
SRCS := ota_main.c \
        HAL-ota/hal_ota.c \
        flashing-firmware/module_manager.c \
        fsm-table-driven/table_driven_fsm.c \
        flashing-firmware/firmware_update.c \
        md5_jiao-yan/md5.c \
        lzma/unzip/unzip.c \
        lzma/zip/zip.c \
        lzma/flow-unzip/unzip_streame.c \
        ../lzma2602/C/Alloc.c \
        ../lzma2602/C/LzmaDec.c \
        ../lzma2602/C/7zCrc.c \
        ../lzma2602/C/7zCrcOpt.c

# 在子目录中搜索源文件，使 $(BUILD_DIR)/xxx.o 能匹配到对应源文件
VPATH := .:fsm-table-driven:HAL-ota:flashing-firmware:md5_jiao-yan:lzma:lzma/unzip:lzma/zip:lzma/flow-unzip:../lzma2602/C

# 目标文件统一输出到 BUILD_DIR，依赖文件(.d)由 -MMD 自动生成
OBJS := $(addprefix $(BUILD_DIR)/, $(notdir $(SRCS:.c=.o)))
DEPS := $(OBJS:.o=.d)

# ---------- 规则 ----------
# 默认目标
all: $(BUILD_DIR)/$(TARGET)
	./$(BUILD_DIR)/$(TARGET)

# 链接
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
