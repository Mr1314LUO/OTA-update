# ============================================================================
# lib/Makefile —— 独立工具构建
# 构建 firmware_create / flow-unzip / unzip / md5sum 四个独立二进制工具
# ============================================================================

# 编译器与工具链
CC       := gcc

# 编译标志
CFLAGS   := -Wall -Wextra -O2 -g
CFLAGS   += -DLOG_LEVEL=3 -DLOG_LOC_ENABLE=1 -DHOST_SIM

# 链接标志
LDFLAGS  := -lm
LDLIBS   := -llzma

# ============================================================================
# 目录定义
# ============================================================================
BUILD_DIR     := build
APP_DIR       := app
SDK_DIR       := lzma/sdk

# ============================================================================
# 头文件搜索路径
# ============================================================================
#   -I.           → 可找到 hal/hal_ota.h, fsm/ota_fsm.h, md5/md5.h,
#                    lzma/zip/zip.h, module-manager/module_manager.h,
#                    update-table/firmware_update.h, lzma/flow-unzip/unzip_stream.h
#   -I$(APP_DIR)  → 可找到应用层头文件 printf.h, ota_main.h
#   -I$(SDK_DIR)  → 可找到 7zCrc.h, Alloc.h, LzmaDec.h, 7zAlloc.h 等 SDK 头文件
INCLUDES := -I. \
            -I$(APP_DIR) \
            -Ihal \
            -I$(SDK_DIR)

# ============================================================================
# 源文件定义
# ============================================================================

# LZMA SDK 模块（LzmaDec 用于流式解压，LzmaEnc 用于压缩）
SDK_SRCS  := $(SDK_DIR)/7zCrc.c \
             $(SDK_DIR)/7zCrcOpt.c \
             $(SDK_DIR)/Alloc.c \
             $(SDK_DIR)/LzmaDec.c

# LZMA 流式解压模块
UNZIP_STREAM_SRCS := lzma/flow-unzip/unzip_stream.c

# LZMA 压缩模块（zip）
ZIP_SRCS  := lzma/zip/zip.c

# LZMA 解压模块（unzip，基于 liblzma）
UNZIP_SRCS := lzma/unzip/unzip.c

# MD5 模块
MD5_SRCS  := md5/md5.c

# ============================================================================
# 目标定义
# ============================================================================

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

# 默认目标：编译所有模块 bin
all: $(BUILD_DIR) $(MODULE_BINS)
	@echo ""
	@echo "============================================"
	@echo " 编译完成！所有模块二进制文件已生成:"
	@echo "============================================"
	@ls -lh $(MODULE_BINS) 2>/dev/null | awk '{print "  " $$NF " (" $$5 ")"}'
	@echo "============================================"

# 创建构建目录
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

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

# 仅编译各模块 bin 文件
modules: $(BUILD_DIR) $(MODULE_BINS)
	@echo "  [DONE] 各模块二进制文件编译完成"

# ============================================================================
# 清理
# ============================================================================
clean:
	@rm -rf $(BUILD_DIR)
	@echo "  [CLEAN] 已清理构建目录"

# ============================================================================
# 伪目标
# ============================================================================
.PHONY: all clean modules
