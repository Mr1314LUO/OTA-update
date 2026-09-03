#include "ota_fsm.h"

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
    // 主机模拟：检查升级包文件并模拟分块下载进度。
    struct stat st;
    if (stat(CHECK_FILE_PATH, &st) != 0) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Package not found: %s", CHECK_FILE_PATH);
        LOG_ERROR("升级包不存在: %s\n" RESET, CHECK_FILE_PATH);
        fsm_handle_event(ctx, EVENT_DOWNLOAD_FAILED);
        return;
    }

    ctx->total_size = (uint32_t)st.st_size;
    ctx->downloaded_size = 0;
    LOG_INFO("🚀 开始下载升级包 (%u bytes)...\n" , ctx->total_size);

    const uint32_t chunk = 4096;    // 模拟分块传输
    while (ctx->downloaded_size < ctx->total_size) {
        uint32_t to_recv = ctx->total_size - ctx->downloaded_size;
        if (to_recv > chunk) {
            to_recv = chunk;
        }
        ctx->downloaded_size += to_recv;
        ctx->progress = (uint8_t)((ctx->downloaded_size * 100) / ctx->total_size);
        LOG_INFO("\r🚀🚀🚀 下载进度: %d%% (%u/%u bytes)" ,
               ctx->progress, ctx->downloaded_size, ctx->total_size);
        fflush(stdout);
        usleep(2000);   // 模拟网络传输延迟
    }
    printf("\n");
    fsm_handle_event(ctx, EVENT_DOWNLOAD_COMPLETE);
}

// MD5 验证升级包完整性
static void action_start_verify(ota_context_t *ctx) {
    // 下载完成后，进行 MD5 完整性校验
    LOG_INFO(" 🔍 开始校验升级包: %s\n" RESET, CHECK_FILE_PATH);
    if (compare_flie(CHECK_FILE_PATH, MD5_PATH) == 0) {
        fsm_handle_event(ctx, EVENT_VERIFY_SUCCESS);
    } else {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "MD5 verify failed");
        fsm_handle_event(ctx, EVENT_VERIFY_FAILED);
    }
}

// 准备升级
static void action_prepare_update(ota_context_t *ctx) {
    // 校验通过后，打包压缩固件并标记准备升级
    LOG_INFO(" ✅ 校验通过，打包压缩固件...\n");
    // 目标版本字符串（如 "V1.1"）原样写入固件包头部
    if (compressed_File(CHECK_FILE_PATH, zip_file_path, g_modules[0].target_version) != 0) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Package compress failed");
        fsm_handle_event(ctx, EVENT_UPDATE_FAILED);
        return;
    }

    // 真实设备：此处可提示用户确认升级；
    // 主机模拟：自动确认升级。
    fsm_handle_event(ctx, EVENT_READY_CONFIRM);
}

// 开始升级
static void action_start_updating(ota_context_t *ctx) {
    // Bootloader 启动后，按分区表执行实际升级操作
    LOG_INFO(" 💙 开始写入固件分区...\n");
    if (firmware_update() == 0) {
        fsm_handle_event(ctx, EVENT_UPDATE_COMPLETE);
    } else {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Firmware update failed");
        fsm_handle_event(ctx, EVENT_UPDATE_FAILED);
    }
}

// 升级成功
static void action_update_success(ota_context_t *ctx) {
    // 升级完成后的清理工作
    ctx->progress = 100;
    LOG_SUCCESS("升级成功，固件版本已更新\n");

    // 真实设备：复位系统，由 Bootloader 引导新固件（主机端为桩实现）
    // hal_ota_instance.system_reset();
}

// 升级失败
static void action_update_failed(ota_context_t *ctx) {
    // 失败处理，记录日志等
    LOG_FAILURE("升级失败: %s\n" RESET,
                ctx->error_msg[0] != '\0' ? ctx->error_msg : "unknown error");
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
