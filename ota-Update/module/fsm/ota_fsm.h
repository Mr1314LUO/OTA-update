#ifndef OTA_FSM_H
#define OTA_FSM_H

#include <stdint.h>
#include <stdbool.h>

#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "printf.h"
#include "md5/md5.h"
#include "lzma/zip/zip.h"
#include "module-manager/module_manager.h"
#include "update-table/firmware_update.h"

// ==========================================
// 表格驱动 OTA 状态机 —— 对外接口
// 本头文件只描述状态/事件/上下文与处理函数，
// 不引入任何底层模块（固件升级/校验/压缩）的实现细节，
// 相关依赖一律下沉到 ota_fsm.c 中
// ==========================================

// 定义OTA事件
typedef enum {
    EVENT_START_CHECK,          // 开始检查更新（终态下再次触发可用于重试）
    EVENT_MANIFEST_SUCCESS,     // 清单获取成功
    EVENT_MANIFEST_FAILED,      // 清单获取失败
    EVENT_UPDATE_AVAILABLE,     // 有可用更新
    EVENT_NO_UPDATE,            // 无可用更新
    EVENT_DOWNLOAD_COMPLETE,    // 下载完成
    EVENT_DOWNLOAD_FAILED,      // 下载失败
    EVENT_VERIFY_SUCCESS,       // 验证成功
    EVENT_VERIFY_FAILED,        // 验证失败
    EVENT_READY_CONFIRM,        // 确认升级
    EVENT_UPDATE_COMPLETE,      // 升级完成
    EVENT_UPDATE_FAILED,        // 升级失败
    EVENT_MAX
} ota_event_t;

// 定义OTA状态
typedef enum {
    OTA_STATE_IDLE,         // 空闲状态，等待用户触发
    OTA_STATE_CHECKING,     // 检查状态，等待检查升级
    OTA_STATE_DOWNLOADING,  // 下载状态，等待下载升级包
    OTA_STATE_VERIFYING,    // 验证状态，等待验证升级包
    OTA_STATE_READY,        // 准备就绪状态，等待升级
    OTA_STATE_UPDATING,     // 升级状态，正在应用升级包
    OTA_STATE_SUCCESS,      // 成功升级状态
    OTA_STATE_FAILED        // 失败升级状态
} ota_state_t;

// 定义状态机上下文结构
typedef struct {
    ota_state_t state;          // 当前状态
    uint32_t total_size;        // 总升级包大小
    uint32_t downloaded_size;   // 已下载大小
    uint8_t progress;           // 进度 0-100
    char error_msg[64];         // 错误信息缓冲区
} ota_context_t;


// 定义动作函数类型
typedef void (*Action)(ota_context_t *ctx);

// 定义转移表条目结构
typedef struct {
    ota_state_t current_state;
    ota_event_t event;
    ota_state_t next_state;
    Action action;      // 动作函数指针
} OtaStateTransition;

// 状态/事件名称（仅用于日志输出）
static const char *state_name(ota_state_t s) {
    switch (s) {
    case OTA_STATE_IDLE:        return "IDLE";
    case OTA_STATE_CHECKING:    return "CHECKING";
    case OTA_STATE_DOWNLOADING: return "DOWNLOADING";
    case OTA_STATE_VERIFYING:   return "VERIFYING";
    case OTA_STATE_READY:       return "READY";
    case OTA_STATE_UPDATING:    return "UPDATING";
    case OTA_STATE_SUCCESS:     return "SUCCESS";
    case OTA_STATE_FAILED:      return "FAILED";
    default:                    return "UNKNOWN";
    }
}
static const char *event_name(ota_event_t e) {
    switch (e) {
    case EVENT_START_CHECK:       return "START_CHECK";
    case EVENT_MANIFEST_SUCCESS:  return "MANIFEST_SUCCESS";
    case EVENT_MANIFEST_FAILED:   return "MANIFEST_FAILED";
    case EVENT_UPDATE_AVAILABLE:  return "UPDATE_AVAILABLE";
    case EVENT_NO_UPDATE:         return "NO_UPDATE";
    case EVENT_DOWNLOAD_COMPLETE: return "DOWNLOAD_COMPLETE";
    case EVENT_DOWNLOAD_FAILED:   return "DOWNLOAD_FAILED";
    case EVENT_VERIFY_SUCCESS:    return "VERIFY_SUCCESS";
    case EVENT_VERIFY_FAILED:     return "VERIFY_FAILED";
    case EVENT_READY_CONFIRM:     return "READY_CONFIRM";
    case EVENT_UPDATE_COMPLETE:   return "UPDATE_COMPLETE";
    case EVENT_UPDATE_FAILED:     return "UPDATE_FAILED";
    default:                      return "UNKNOWN";
    }
}


// 事件处理：查找转移表并执行动作、状态转移
void fsm_handle_event(ota_context_t *ctx, ota_event_t event);

// 周期处理：根据当前状态自动触发对应事件
void ota_engine_process(ota_context_t *ctx);

// 动作函数前置声明（实现见下文）
static void action_start_check(ota_context_t *ctx);
static void action_fetch_manifest(ota_context_t *ctx);
static void action_start_download(ota_context_t *ctx);
static void action_start_verify(ota_context_t *ctx);
static void action_prepare_update(ota_context_t *ctx);
static void action_start_updating(ota_context_t *ctx);
static void action_update_success(ota_context_t *ctx);
static void action_update_failed(ota_context_t *ctx);
static void action_no_update(ota_context_t *ctx);

#endif // OTA_FSM_H
