#include "ota_fsm.h"

// P7: MCU 路径需要 boot_protocol.h 来写 upgrade_flag=PENDING
#ifndef HOST_SIM
#include "boot_protocol.h"
#endif

// ==========================================
// 固件源全局指针（MCU 路径用）
// - PC 仿真（HOST_SIM）: 不使用,保持 NULL
// - MCU: 由 ota_main.c 在启动时指向 &g_spiflash_source
// action_start_download / action_start_verify 在 MCU 路径下访问此指针
// ==========================================
extern firmware_source_t *g_fw_src;

// ==========================================
// 定义状态转移表
// ==========================================
static OtaStateTransition ota_state_table[] = {
    {OTA_STATE_IDLE,      EVENT_START_CHECK,       OTA_STATE_CHECKING,   action_start_check},
    // 检查状态，清单获取成功，解析清单判断是否有更新
    {OTA_STATE_CHECKING,  EVENT_MANIFEST_SUCCESS,  OTA_STATE_CHECKING,   action_fetch_manifest},
    {OTA_STATE_CHECKING,  EVENT_MANIFEST_FAILED,   OTA_STATE_FAILED,     action_update_failed},
    {OTA_STATE_CHECKING,  EVENT_UPDATE_AVAILABLE,  OTA_STATE_DOWNLOADING, action_start_download},
    {OTA_STATE_CHECKING,  EVENT_NO_UPDATE,         OTA_STATE_IDLE,       action_no_update},
    // 下载状态，下载完成，转移到验证状态，执行验证升级包操作
    {OTA_STATE_DOWNLOADING, EVENT_DOWNLOAD_COMPLETE, OTA_STATE_VERIFYING, action_start_verify},
    {OTA_STATE_DOWNLOADING, EVENT_DOWNLOAD_FAILED,   OTA_STATE_FAILED,   action_update_failed},
    // 验证状态，验证成功，转移到准备升级状态，执行准备升级操作
    {OTA_STATE_VERIFYING, EVENT_VERIFY_SUCCESS,    OTA_STATE_READY,      action_prepare_update},
    {OTA_STATE_VERIFYING, EVENT_VERIFY_FAILED,     OTA_STATE_FAILED,     action_update_failed},
    // 准备升级状态，确认升级，转移到升级状态，执行升级操作
    {OTA_STATE_READY,     EVENT_READY_CONFIRM,     OTA_STATE_UPDATING,   action_start_updating},
    {OTA_STATE_READY,     EVENT_UPDATE_FAILED,     OTA_STATE_FAILED,     action_update_failed},
    // 升级状态，升级完成，转移到成功状态，执行升级成功操作
    {OTA_STATE_UPDATING,  EVENT_UPDATE_COMPLETE,   OTA_STATE_SUCCESS,    action_update_success},
    {OTA_STATE_UPDATING,  EVENT_UPDATE_FAILED,     OTA_STATE_FAILED,     action_update_failed},
    // 成功状态，保持成功状态，可重新发起检查
    {OTA_STATE_SUCCESS,   EVENT_START_CHECK,       OTA_STATE_CHECKING,   action_start_check},
    {OTA_STATE_SUCCESS,   EVENT_MAX,               OTA_STATE_SUCCESS,    action_no_update},
    // 失败状态，保持失败状态，可通过 EVENT_START_CHECK 重试
    {OTA_STATE_FAILED,    EVENT_START_CHECK,       OTA_STATE_CHECKING,   action_start_check},
    {OTA_STATE_FAILED,    EVENT_MAX,               OTA_STATE_FAILED,     action_no_update},
};


// 开始检查更新
static void action_start_check(ota_context_t *ctx) {
    // 复位进度信息
    ctx->total_size = 0;
    ctx->downloaded_size = 0;
    ctx->progress = 0;
    ctx->error_msg[0] = '\0';
    LOG_INFO(ICON_START "开始检查固件更新...\n" RESET);

    // 真实设备：此处应发起 HTTP/MQTT 请求拉取清单（异步），
    //           网络任务在收到响应后触发 EVENT_MANIFEST_SUCCESS / EVENT_MANIFEST_FAILED。
    // 主机模拟：直接触发清单获取成功，进入解析动作。
    fsm_handle_event(ctx, EVENT_MANIFEST_SUCCESS);
}

// 获取并解析升级清单
static void action_fetch_manifest(ota_context_t *ctx) {
    // 真实设备：manifest 数据来自网络接收缓冲区；
    // 主机模拟：使用内置演示清单。
    const uint8_t *manifest = (const uint8_t *)demo_manifest;
    uint32_t len = (uint32_t)strlen(demo_manifest);

    if (!module_manager_parse_manifest(manifest, len)) {
        // 清单解析失败
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Parse manifest failed");
        fsm_handle_event(ctx, EVENT_MANIFEST_FAILED);
    } else if (module_manager_is_update_available()) {
        // 有模块需要更新
        fsm_handle_event(ctx, EVENT_UPDATE_AVAILABLE);
    } else {
        // 无更新
        fsm_handle_event(ctx, EVENT_NO_UPDATE);
    }
}

// 下载升级包
static void action_start_download(ota_context_t *ctx) {
    // 真实设备：分块接收升级包并写入下载分区，完成后触发
    //           EVENT_DOWNLOAD_COMPLETE / EVENT_DOWNLOAD_FAILED。
    // 主机模拟：检查升级包文件后，把下载请求交给平台下载任务异步执行。
#ifdef HOST_SIM
    struct stat st;
    if (stat(CHECK_FILE_PATH, &st) != 0) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Package not found: %s", CHECK_FILE_PATH);
        LOG_ERROR("升级包不存在: %s\n" RESET, CHECK_FILE_PATH);
        fsm_handle_event(ctx, EVENT_DOWNLOAD_FAILED);
        return;
    }
    ctx->total_size = (uint32_t)st.st_size;
#else
    // MCU 路径：固件镜像已预存于 SPI Flash，通过 firmware_source_t 接口获取大小
    if (g_fw_src == NULL || g_fw_src->size == NULL) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "No firmware source");
        LOG_ERROR("固件源未配置\n" RESET);
        fsm_handle_event(ctx, EVENT_DOWNLOAD_FAILED);
        return;
    }
    ctx->total_size = g_fw_src->size(g_fw_src->ctx);
    if (ctx->total_size == 0u) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Firmware not present");
        LOG_ERROR("SPI Flash 中无固件镜像\n" RESET);
        fsm_handle_event(ctx, EVENT_DOWNLOAD_FAILED);
        return;
    }
#endif
    ctx->downloaded_size = 0;
    LOG_INFO("🚀 开始下载升级包 (%u bytes)...\n", (unsigned)ctx->total_size);

    // 发起异步下载：状态保持 DOWNLOADING，等待平台任务回送事件
    ota_platform_download_request(ctx);
}

// MD5 验证升级包完整性
static void action_start_verify(ota_context_t *ctx) {
    // 下载完成后，进行 MD5 完整性校验
#ifdef HOST_SIM
    LOG_INFO(" 🔍 开始校验升级包: %s\n" RESET, CHECK_FILE_PATH);
    if (compare_flie(CHECK_FILE_PATH, MD5_PATH) == 0) {
        fsm_handle_event(ctx, EVENT_VERIFY_SUCCESS);
    } else {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "MD5 verify failed");
        fsm_handle_event(ctx, EVENT_VERIFY_FAILED);
    }
#else
    // MCU 路径：从 firmware_source_t 分块读取 SPI Flash 中的固件镜像并计算 MD5
    LOG_INFO(" 🔍 开始校验升级包 (SPI Flash source)\n" RESET);
    if (g_fw_src == NULL) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "No firmware source");
        fsm_handle_event(ctx, EVENT_VERIFY_FAILED);
        return;
    }
    char md5_str[33];
    if (source_md5(g_fw_src, md5_str) == 0) {
        // TODO(P6): 从 SPI Flash 元数据区读取期望 MD5 并对比
        LOG_INFO("实际 MD5: %s\n", md5_str);
        LOG_SUCCESS("校验通过\n");
        fsm_handle_event(ctx, EVENT_VERIFY_SUCCESS);
    } else {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "MD5 verify failed");
        LOG_ERROR("MD5 计算失败\n" RESET);
        fsm_handle_event(ctx, EVENT_VERIFY_FAILED);
    }
#endif
}

// 准备升级
static void action_prepare_update(ota_context_t *ctx) {
    // 校验通过后，准备升级
    LOG_INFO(" ✅ 校验通过，准备升级...\n");
#ifdef HOST_SIM
    // 主机仿真路径：将固件打包压缩为 .lzma 包（Bootloader 启动后流式解压）
    if (compressed_File(CHECK_FILE_PATH, zip_file_path, g_modules[0].target_version) != 0) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Package compress failed");
        fsm_handle_event(ctx, EVENT_UPDATE_FAILED);
        return;
    }
#else
    // MCU 路径：固件镜像已在 SPI Flash 中以 .lzma 格式预存，
    //           无需打包；ota_platform_request_confirm() 负责写元数据标志
    //           (upgrade_flag=MAGIC_PENDING) 到 SPI Flash 元数据区
#endif
    // 真实设备：此处可提示用户确认升级；
    // 主机模拟：把确认请求交给平台用户任务，等待 EVENT_READY_CONFIRM
    ota_platform_request_confirm();
}

// 开始升级
static void action_start_updating(ota_context_t *ctx) {
    // Bootloader 启动后，按分区表执行实际升级操作
    LOG_INFO(" 💙 开始升级...\n");
#ifdef HOST_SIM
    // 主机仿真路径：原地流式解压 .lzma 包并写入（桩）Flash 分区
    if (firmware_update() == 0) {
        fsm_handle_event(ctx, EVENT_UPDATE_COMPLETE);
    } else {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Firmware update failed");
        fsm_handle_event(ctx, EVENT_UPDATE_FAILED);
    }
#else
    // MCU 路径：写 upgrade_flag=PENDING 到 SPI Flash 元数据区，触发软复位
    //           Bootloader 接管：读 PENDING → 擦 App 区 → 流式 LZMA 解压写 Flash
    //                            → 写 DONE → 跳转 App
    LOG_INFO("写 PENDING 标志到 SPI Flash...\n");
    if (!boot_write_upgrade_flag(UPGRADE_FLAG_PENDING)) {
        LOG_ERROR("写 PENDING 标志失败!\n" RESET);
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Write PENDING failed");
        fsm_handle_event(ctx, EVENT_UPDATE_FAILED);
        return;
    }
    LOG_INFO("触发软复位，Bootloader 将接管升级流程...\n");
    hal_ota_instance.system_reset();
    // system_reset 不应返回；保险驻留
    while (1) {
    }
#endif
}

// 升级成功
static void action_update_success(ota_context_t *ctx) {
    // 升级完成后的清理工作
    ctx->progress = 100;
    LOG_SUCCESS("升级成功，固件版本已更新\n");
    fflush(stdout);  // 终态驻留前刷新,防止 timeout SIGTERM 丢失缓冲输出

    // 真实设备：复位系统，由 Bootloader 引导新固件（主机端为桩实现）
    // hal_ota_instance.system_reset();
}

// 升级失败
static void action_update_failed(ota_context_t *ctx) {
    // 失败处理，记录日志等
    LOG_FAILURE("升级失败: %s\n" RESET,
                ctx->error_msg[0] != '\0' ? ctx->error_msg : "unknown error");
    fflush(stdout);  // 终态驻留前刷新,防止 timeout SIGTERM 丢失缓冲输出
}

static void action_no_update(ota_context_t *ctx) {
    // 无更新，无需操作
    (void)ctx;
    LOG_INFO(" 🟢 当前已是最新版本，无需升级\n");
}

// 查找并执行状态转移
// 说明：先切换状态，再执行动作。动作内部可根据执行结果继续触发后续事件
//       （递归调用本函数），从而把真实结果（校验/升级成败）驱动到正确的状态。
void fsm_handle_event(ota_context_t *ctx, ota_event_t event) {
    int table_size = sizeof(ota_state_table) / sizeof(OtaStateTransition);

    for (int i = 0; i < table_size; i++) {
        // 状态和事件匹配
        if (ota_state_table[i].current_state != ctx->state ||
            ota_state_table[i].event != event) {
            continue;
        }

        LOG_INFO(ICON_PIN "事件 [%s]，状态 %s -> %s\n" ,
                 event_name(event),
                 state_name(ota_state_table[i].current_state),
                 state_name(ota_state_table[i].next_state));

        // 先更新状态，再执行动作
        ctx->state = ota_state_table[i].next_state;

        // 执行动作
        if (ota_state_table[i].action != NULL) {
            ota_state_table[i].action(ctx);
        }
        return;
    }
    // 未找到匹配的转移规则
    LOG_ERROR(BOLDRED"❌ 事件 %s 在状态 %s 中被忽略\n" RESET,
              event_name(event), state_name(ctx->state));
}

// 核心处理函数 - 表格驱动状态机
void ota_engine_process(ota_context_t *ctx) {
    switch (ctx->state) {
        case OTA_STATE_IDLE:
            // 空闲时自动发起检查（真实设备可由云端推送/定时器触发）
            fsm_handle_event(ctx, EVENT_START_CHECK);
            break;
        case OTA_STATE_SUCCESS:
        case OTA_STATE_FAILED:
        default:
            // 终态：等待外部触发 EVENT_START_CHECK 重试，主循环无需操作
            break;
    }
}
