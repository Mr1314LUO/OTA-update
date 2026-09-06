# ============================================================================
# OTA-Update 项目 Makefile
# 功能：编译各个模块生成独立的二进制文件，支持主机模拟运行
# ============================================================================

# 编译器与工具链
CC       := gcc
OBJCOPY  := objcopy

# 编译标志
CFLAGS   := -Wall -Wextra -O2 -g
CFLAGS   += -DLOG_LEVEL=3 -DLOG_LOC_ENABLE=1

# 链接标志
LDFLAGS  := -lm
LDLIBS   := -llzma

# ============================================================================
# 目录定义
# ============================================================================
BUILD_DIR     := build
APP_DIR       := app
HAL_DIR       := hal
MODULE_DIR    := module
SDK_DIR       := $(MODULE_DIR)/lzma/sdk

# ============================================================================
# 头文件搜索路径
# ============================================================================
# 头文件搜索路径说明：
#   -I.               → 可找到 hal/hal_ota.h
#   -I$(APP_DIR)      → 可找到应用层头文件 printf.h, ota_main.h
#   -I$(MODULE_DIR)   → 可找到 fsm/ota_fsm.h, md5/md5.h, lzma/zip/zip.h,
#                        module-manager/module_manager.h,
#                        update-table/firmware_update.h,
#                        lzma/flow-unzip/unzip_stream.h
#   -I$(SDK_DIR)      → 可找到 7zCrc.h, Alloc.h, LzmaDec.h, 7zAlloc.h 等 SDK 头文件
INCLUDES := -I. \
            -I$(APP_DIR) \
            -I$(MODULE_DIR) \
            -I$(SDK_DIR)

# ============================================================================
# 源文件定义
# ============================================================================

# HAL 模块
HAL_SRCS  := $(HAL_DIR)/hal_ota.c
HAL_OBJS  := $(BUILD_DIR)/hal_ota.o

# LZMA SDK 模块（LzmaDec 用于流式解压，LzmaEnc 用于压缩）
SDK_SRCS  := $(SDK_DIR)/7zCrc.c \
             $(SDK_DIR)/7zCrcOpt.c \
             $(SDK_DIR)/Alloc.c \
             $(SDK_DIR)/LzmaDec.c
SDK_OBJS  := $(BUILD_DIR)/7zCrc.o \
             $(BUILD_DIR)/7zCrcOpt.o \
             $(BUILD_DIR)/Alloc.o \
             $(BUILD_DIR)/LzmaDec.o

# LZMA 流式解压模块
UNZIP_STREAM_SRCS := $(MODULE_DIR)/lzma/flow-unzip/unzip_stream.c
UNZIP_STREAM_OBJS := $(BUILD_DIR)/unzip_stream.o

# LZMA 压缩模块（zip）
ZIP_SRCS  := $(MODULE_DIR)/lzma/zip/zip.c
ZIP_OBJS  := $(BUILD_DIR)/zip.o

# LZMA 解压模块（unzip，基于 liblzma）
UNZIP_SRCS := $(MODULE_DIR)/lzma/unzip/unzip.c
UNZIP_OBJS := $(BUILD_DIR)/unzip.o

# MD5 模块
MD5_SRCS  := $(MODULE_DIR)/md5/md5.c
MD5_OBJS  := $(BUILD_DIR)/md5.o

# 模块管理模块
MODULE_MGR_SRCS := $(MODULE_DIR)/module-manager/module_manager.c
MODULE_MGR_OBJS := $(BUILD_DIR)/module_manager.o

# 固件升级表模块
UPDATE_TABLE_SRCS := $(MODULE_DIR)/update-table/firmware_update.c
UPDATE_TABLE_OBJS := $(BUILD_DIR)/firmware_update.o

# FSM 状态机模块
FSM_SRCS  := $(MODULE_DIR)/fsm/ota_fsm.c
FSM_OBJS  := $(BUILD_DIR)/ota_fsm.o

# 主应用
APP_SRCS  := $(APP_DIR)/ota_main.c
APP_OBJS  := $(BUILD_DIR)/ota_main.o

# ============================================================================
# 目标定义
# ============================================================================

# 主应用（FreeRTOS 多任务版本，由根目录 Makefile 构建并集成 FreeRTOS 内核）
TARGET_OTA := ../build/ota_main

# 各模块独立可执行文件（bin 文件）
TARGET_FIRMWARE_CREATE := $(BUILD_DIR)/firmware_create
TARGET_FLOW_UNZIP      := $(BUILD_DIR)/flow-unzip
TARGET_UNZIP           := $(BUILD_DIR)/unzip
TARGET_MD5SUM          := $(BUILD_DIR)/md5sum

# 所有模块 bin 文件列表
MODULE_BINS := $(TARGET_FIRMWARE_CREATE) \
              $(TARGET_FLOW_UNZIP) \
              $(TARGET_UNZIP) \
              $(TARGET_MD5SUM)

# 默认目标：编译所有（模块 bin；主应用 ota_main 由根目录 Makefile 构建）
all: $(BUILD_DIR) $(MODULE_BINS)
	@echo ""
	@echo "============================================"
	@echo " 编译完成！所有模块二进制文件已生成:"
	@echo "============================================"
	@ls -lh $(TARGET_OTA) $(MODULE_BINS) 2>/dev/null | awk '{print "  " $$NF " (" $$5 ")"}'
	@echo "============================================"

# 创建构建目录
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# ============================================================================
# 通用编译规则：将 .c 编译为 .o
# ============================================================================
$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(HAL_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(APP_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(MODULE_DIR)/fsm/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(MODULE_DIR)/md5/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(MODULE_DIR)/module-manager/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(MODULE_DIR)/update-table/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(MODULE_DIR)/lzma/zip/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(MODULE_DIR)/lzma/flow-unzip/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(MODULE_DIR)/lzma/unzip/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(MODULE_DIR)/lzma/sdk/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ============================================================================
# 主应用 OTA 链接（已集成 FreeRTOS，委托根目录 Makefile 构建）
# ============================================================================
$(TARGET_OTA):
	@$(MAKE) -C .. build/ota_main
	@echo "  [LINK] $(TARGET_OTA)"

# ============================================================================
# 模块独立可执行文件（bin 文件）
# ============================================================================

# firmware_create: 固件打包工具（zip 模块独立编译）
$(TARGET_FIRMWARE_CREATE): $(ZIP_SRCS) $(SDK_SRCS)
	$(CC) -DENABLE_ZIP_CLI $(CFLAGS) $(INCLUDES) $(ZIP_SRCS) $(SDK_SRCS) $(LDFLAGS) $(LDLIBS) -o $@
	@echo "  [BUILD] $(TARGET_FIRMWARE_CREATE)"

# flow-unzip: 流式解压工具（基于 LZMA SDK）
$(TARGET_FLOW_UNZIP): $(UNZIP_STREAM_SRCS) $(SDK_SRCS)
	$(CC) -DFLOW_UNZIP_STANDALONE $(CFLAGS) $(INCLUDES) $(UNZIP_STREAM_SRCS) $(SDK_SRCS) $(LDFLAGS) -o $@
	@echo "  [BUILD] $(TARGET_FLOW_UNZIP)"

# unzip: 解压工具（基于 liblzma）
$(TARGET_UNZIP): $(UNZIP_SRCS)
	$(CC) -DUNZIP_STANDALONE $(CFLAGS) $(INCLUDES) $(UNZIP_SRCS) $(LDFLAGS) $(LDLIBS) -o $@
	@echo "  [BUILD] $(TARGET_UNZIP)"

# md5sum: MD5 校验工具
$(TARGET_MD5SUM): $(MD5_SRCS)
	$(CC) -DENABLE_MD5_CLI $(CFLAGS) $(INCLUDES) $(MD5_SRCS) $(LDFLAGS) -o $@
	@echo "  [BUILD] $(TARGET_MD5SUM)"

# ============================================================================
# 便捷目标
# ============================================================================

# 仅编译主应用（委托根目录 Makefile：FreeRTOS 多任务版本）
ota: $(TARGET_OTA)
	@echo "  [DONE] OTA 主应用编译完成"

# 仅编译各模块 bin 文件
modules: $(BUILD_DIR) $(MODULE_BINS)
	@echo "  [DONE] 各模块二进制文件编译完成"

# 运行 OTA 主应用
run: $(TARGET_OTA)
	@$(MAKE) -C .. run

# ============================================================================
# 清理
# ============================================================================
clean:
	@rm -rf $(BUILD_DIR)
	@echo "  [CLEAN] 已清理构建目录"

# ============================================================================
# 伪目标
# ============================================================================
.PHONY: all clean ota modules run