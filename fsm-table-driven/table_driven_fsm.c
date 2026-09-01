#include "table_driven_fsm.h"

// ==========================================
// 定义状态转移表
// ==========================================
static OtaStateTransition ota_state_table[] = {
    {OTA_STATE_IDLE,      EVENT_START_CHECK,      OTA_STATE_CHECKING,   action_start_check},
    {OTA_STATE_CHECKING,  EVENT_MANIFEST_SUCCESS, OTA_STATE_CHECKING,   action_fetch_manifest},
    {OTA_STATE_CHECKING,  EVENT_MANIFEST_FAILED,  OTA_STATE_FAILED,     action_update_failed},
    {OTA_STATE_CHECKING,  EVENT_UPDATE_AVAILABLE, OTA_STATE_DOWNLOADING, action_start_download},
    {OTA_STATE_CHECKING,  EVENT_NO_UPDATE,        OTA_STATE_IDLE,       action_no_update},
    // 下载状态，下载完成，转移到验证状态，执行验证升级包操作
    {OTA_STATE_DOWNLOADING, EVENT_DOWNLOAD_COMPLETE, OTA_STATE_VERIFYING, action_start_verify},
    // 验证状态，验证成功，转移到准备升级状态，执行准备升级操作
    {OTA_STATE_VERIFYING, EVENT_VERIFY_SUCCESS,   OTA_STATE_READY,      action_prepare_update},
    // 准备升级状态，确认升级，转移到升级状态，执行升级操作
    {OTA_STATE_READY,     EVENT_READY_CONFIRM,    OTA_STATE_UPDATING,   action_start_updating},
    // 升级状态，升级完成，转移到成功状态，执行升级成功操作
    {OTA_STATE_UPDATING,  EVENT_UPDATE_COMPLETE,  OTA_STATE_SUCCESS,    action_update_success},
    // 成功状态，保持成功状态
    {OTA_STATE_SUCCESS,   EVENT_MAX,              OTA_STATE_SUCCESS,    action_no_update},
    // 失败状态，保持失败状态
    {OTA_STATE_FAILED,    EVENT_MAX,              OTA_STATE_FAILED,     action_no_update},
};

// 开始检查更新
void action_start_check(ota_context_t *ctx) {
    // 开始检查更新，无需额外操作
    (void)ctx;
}

// 模拟从服务器获取升级清单
void action_fetch_manifest(ota_context_t *ctx) {
    // 模拟从服务器获取升级清单
    uint8_t manifest[1024] = {0};
    uint32_t len = 0;

    // 这里应该有实际的HTTP/MQTT下载逻辑

    bool success = true; // 模拟成功

    if (success) {
        // 解析清单，判断是否有模块需要更新
        if (module_manager_parse_manifest(manifest, len) &&
            module_manager_is_update_available()) {
            // 有更新，触发UPDATE_AVAILABLE事件
            fsm_handle_event(ctx, EVENT_UPDATE_AVAILABLE);
        } else {
            // 无更新，触发NO_UPDATE事件
            fsm_handle_event(ctx, EVENT_NO_UPDATE);
        }
    } else {
        // 获取失败，触发MANIFEST_FAILED事件
        strcpy(ctx->error_msg, "Fetch manifest failed");
        fsm_handle_event(ctx, EVENT_MANIFEST_FAILED);
    }
}

// 开始下载升级包
void action_start_download(ota_context_t *ctx) {
    // // 初始化下载参数
    // ctx->total_size = 1024 * 50; // 假设总大小50KB
    // ctx->downloaded_size = 0;
    // ctx->progress = 0;
    // // 实际下载逻辑应该在这里或单独的任务中执行
    (void)ctx;
}

// 开始验证升级包
void action_start_verify(ota_context_t *ctx) {
    // 下载完成后，进行完整性校验
    // 校验逻辑应该在这里执行
    (void)ctx;
    LOG_BLUE_DOT("action_start_verify\n");
    compare_flie(CHECK_FILE_PATH, MD5_PATH);
}

// const char *unzip_file_path;
// 准备升级
void action_prepare_update(ota_context_t *ctx) {
    // 校验通过后，标记准备升级
    // 设置升级标志等准备工作
    (void)ctx;
    LOG_BLUE_DOT("action_prepare_update\n");
    //压缩固件包
    compressed_File(CHECK_FILE_PATH, zip_file_path, "1.0");
}
// 开始升级
void action_start_updating(ota_context_t *ctx) {
    // Bootloader启动后，执行实际升级操作
    // 此部分通常在Bootloader中完成
    (void)ctx;
    LOG_BLUE_DOT("action_start_updating\n");
    firmware_update();
}
// 升级成功
void action_update_success(ota_context_t *ctx) {
    // 升级完成后的清理工作
    (void)ctx;
    LOG_SUCCESS("action_update_success\n");
}
// 升级失败
void action_update_failed(ota_context_t *ctx) {
    // 失败处理，记录日志等
    (void)ctx;
    LOG_FAILURE("action_update_failed\n");}

void action_no_update(ota_context_t *ctx) {
    // 无更新，无需操作
    (void)ctx;
    LOG_BLUE_DOT("action_no_update\n");
}

// 查找并执行状态转移
void fsm_handle_event(ota_context_t *ctx, ota_event_t event) {
    int table_size = sizeof(ota_state_table) / sizeof(OtaStateTransition);

    for (int i = 0; i < table_size; i++) {
        // 状态和事件匹配
        int match = (ota_state_table[i].current_state == ctx->state) &&
                    (ota_state_table[i].event == event);

        if (!match) {
            LOG_DEBUG(BOLDYELLOW"🔍 查找匹配的转移规则: %d\n" RESET, match);
            continue;
        }

        // 执行动作
        if (ota_state_table[i].action != NULL) {
            LOG_INFO(BLUE"📌 收到事件\n" RESET);
            ota_state_table[i].action(ctx);
        }

        // 更新状态
        ctx->state = ota_state_table[i].next_state;
        LOG_INFO(ICON_INFO"State transitioned to: %d\n" RESET, ctx->state);
        return;
    }
    //未找到匹配的转移规则
    LOG_ERROR(BOLDRED"❌ Event %d ignored in state %d\n" RESET, event, ctx->state);
}

// 核心处理函数 - 表格驱动状态机
void ota_engine_process(ota_context_t *ctx) {
    switch (ctx->state) {
        case OTA_STATE_IDLE:
            fsm_handle_event(ctx, EVENT_START_CHECK);
            break;
        case OTA_STATE_CHECKING:
            fsm_handle_event(ctx, EVENT_MANIFEST_SUCCESS);
            break;
        case OTA_STATE_DOWNLOADING:
            fsm_handle_event(ctx, EVENT_DOWNLOAD_COMPLETE);
            break;
        case OTA_STATE_VERIFYING:
            fsm_handle_event(ctx, EVENT_VERIFY_SUCCESS);
            break;
        case OTA_STATE_READY:
            fsm_handle_event(ctx, EVENT_READY_CONFIRM);
            break;
        case OTA_STATE_UPDATING:
            fsm_handle_event(ctx, EVENT_UPDATE_COMPLETE);
            break;
        case OTA_STATE_SUCCESS:
        case OTA_STATE_FAILED:
        default:
            // 终态，无需操作
            break;
    }
}

// int main() {
//     // 初始化状态机(初始状态为IDLE,转移表大小为4)
//     FSM fsm = {STATE_IDLE, sizeof(state_table) / sizeof(StateTransform)};

//     LOG_INFO(BOLDBLUE"\nEvent transitioned:0:启动,1:停止,2:超时\n" RESET);
//     LOG_INFO(BOLDBLUE"\nState transitioned:0:空闲,1:运行,2:停止\n" RESET);

//     fsm_handle_event(&fsm, EVENT_START);    // 启动 -> 启动电机动作，状态切换到RUNNING
//     fsm_handle_event(&fsm, EVENT_TIMEOUT);  // 超时 -> 无动作，状态不变
//     fsm_handle_event(&fsm, EVENT_STOP);     // 停止 -> 停止电机动作，状态切换到STOPPED
//     fsm_handle_event(&fsm, EVENT_TIMEOUT);  // 超时 -> 无动作，状态不变


//     return 0;
// }