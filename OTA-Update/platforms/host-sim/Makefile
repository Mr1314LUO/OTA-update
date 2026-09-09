# ==========================================
# OTA-update 项目 Makefile（host-sim 平台）
# 构建 host-sim 平台的 OTA 仿真应用
# 可在本目录或项目根目录下运行
# ==========================================

# ---------- 项目根目录 ----------
# 使用当前工作目录做基准，避免在不同调用路径下出现解析偏差
PROJECT_ROOT := $(abspath $(CURDIR)/../..)

# ---------- 工具链 ----------
# 本地编译使用 gcc；交叉编译可改为:
#   CC = arm-none-eabi-gcc
CC      ?= gcc

# ---------- 编译选项 ----------
# 通用编译选项: 开启警告、C11 标准、调试信息
# -pthread: FreeRTOS POSIX 移植层需要（编译与链接均需携带）
CFLAGS  ?= -Wall -Wextra -std=c11 -g
CFLAGS  += -pthread

# 头文件搜索路径（相对于 PROJECT_ROOT）
CPPFLAGS += -I$(PROJECT_ROOT)/lib \
            -I$(PROJECT_ROOT)/platforms/host-sim/app \
            -I$(PROJECT_ROOT)/lib/hal \
            -I$(PROJECT_ROOT)/lib/lzma/sdk \
            -I$(PROJECT_ROOT)/os/freertos/include \
            -I$(PROJECT_ROOT)/platforms/host-sim \
            -I$(PROJECT_ROOT)/platforms/host-sim/freertos \
            -D_POSIX_C_SOURCE=200809L \
            -D_DEFAULT_SOURCE \
            -DUSE_FREERTOS \
            -DHOST_SIM

# ---------- 目标、源文件与构建目录 ----------
TARGET    := ota_main
BUILD_DIR := $(PROJECT_ROOT)/build

# 源文件（相对于 PROJECT_ROOT）
# HOST_SIM 主入口实际位于 lib/ota/ota.c；之前指向的 app/main.c 已不存在。
SRCS := $(PROJECT_ROOT)/lib/ota/ota.c \
        $(PROJECT_ROOT)/platforms/host-sim/hal/hal_ota.c \
        $(PROJECT_ROOT)/lib/fsm/ota_fsm.c \
        $(PROJECT_ROOT)/lib/update-table/firmware_update.c \
        $(PROJECT_ROOT)/lib/module-manager/module_manager.c \
        $(PROJECT_ROOT)/lib/md5/md5.c \
        $(PROJECT_ROOT)/lib/lzma/unzip/unzip.c \
        $(PROJECT_ROOT)/lib/lzma/zip/zip.c \
        $(PROJECT_ROOT)/lib/lzma/flow-unzip/unzip_stream.c \
        $(PROJECT_ROOT)/lib/lzma/sdk/Alloc.c \
        $(PROJECT_ROOT)/lib/lzma/sdk/LzmaDec.c \
        $(PROJECT_ROOT)/lib/lzma/sdk/7zCrc.c \
        $(PROJECT_ROOT)/lib/lzma/sdk/7zCrcOpt.c \
        $(PROJECT_ROOT)/os/freertos/list.c \
        $(PROJECT_ROOT)/os/freertos/queue.c \
        $(PROJECT_ROOT)/os/freertos/tasks.c \
        $(PROJECT_ROOT)/os/freertos/timers.c \
        $(PROJECT_ROOT)/os/freertos/event_groups.c \
        $(PROJECT_ROOT)/os/freertos/heap_4.c \
        $(PROJECT_ROOT)/platforms/host-sim/freertos/port.c \
        $(PROJECT_ROOT)/platforms/host-sim/freertos/utils/wait_for_event.c

# 目标文件统一输出到 BUILD_DIR，保持目录结构
OBJS := $(patsubst $(PROJECT_ROOT)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# ---------- 规则 ----------
# 默认目标：仅完成构建
all: $(BUILD_DIR)/$(TARGET)
	cd $(PROJECT_ROOT) && ./build/$(TARGET)

# 链接（unzip.c 依赖系统 liblzma）
$(BUILD_DIR)/$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ -llzma

# 编译规则
$(BUILD_DIR)/%.o: $(PROJECT_ROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# 运行程序
run: $(BUILD_DIR)/$(TARGET)
	cd $(PROJECT_ROOT) && ./build/$(TARGET)

# 清理编译产物
clean:
	rm -rf $(BUILD_DIR)

# P8: PC 仿真回归测试
regression: $(BUILD_DIR)/$(TARGET)
	@echo "=== P8: PC 仿真回归 ==="
	@rm -f /tmp/ota_p8.log
	@cd $(PROJECT_ROOT) && timeout 90s ./build/$(TARGET) > /tmp/ota_p8.log 2>&1; \
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