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
CPPFLAGS += -I. -IOTA-main -Ifsm-table-driven -Ihal-ota -Iincremental-update -Iflashing-firmware

# ---------- 目标、源文件与构建目录 ----------
TARGET    := ota_main
BUILD_DIR := Make

# 源文件（按实际目录分布）；ota_engine 的函数分别实现在
# OTA-main/ota_main.c 与 fsm-table-driven/table_driven_fsm.c 中
SRCS := OTA-main/ota_main.c \
        hal-ota/hal_ota.c \
        incremental-update/module_manager.c \
        fsm-table-driven/table_driven_fsm.c \
        flashing-firmware/firmware_update.c

# 目标文件统一输出到 BUILD_DIR，依赖文件(.d)由 -MMD 自动生成
OBJS := $(addprefix $(BUILD_DIR)/, $(notdir $(SRCS:.c=.o)))
DEPS := $(OBJS:.o=.d)

# 在子目录中搜索源文件，使 $(BUILD_DIR)/xxx.o 能匹配到对应源文件
VPATH := OTA-main:fsm-table-driven:hal-ota:incremental-update:flashing-firmware

# ---------- 规则 ----------
# 默认目标
all: $(BUILD_DIR)/$(TARGET)
	./$(BUILD_DIR)/$(TARGET)

# 链接
$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# 编译（模式规则）+ 自动依赖生成（-MMD -MP）
# Order-Only 前提 | $(BUILD_DIR) 确保目录存在但不触发重建
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
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
