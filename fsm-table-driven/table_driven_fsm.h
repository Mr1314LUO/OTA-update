#ifndef TABLE_DRIVEN_FSM_H
#define TABLE_DRIVEN_FSM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "printf.h"

#include "firmware_update.h"
#include "md5.h"
#include "zip.h"
#include "unzip_streame.h"

// 前向声明ota_context结构体
// typedef struct ota_context ota_context;

// 定义OTA事件
typedef enum {
    EVENT_START_CHECK,          // 开始检查更新
    EVENT_MANIFEST_SUCCESS,     // 清单获取成功
    EVENT_MANIFEST_FAILED,      // 清单获取失败
    EVENT_UPDATE_AVAILABLE,     // 有可用更新
    EVENT_NO_UPDATE,            // 无可用更新
    EVENT_DOWNLOAD_COMPLETE,    // 下载完成
    EVENT_VERIFY_SUCCESS,       // 验证成功
    EVENT_READY_CONFIRM,        // 确认升级
    EVENT_UPDATE_COMPLETE,      // 升级完成
    EVENT_MAX
} ota_event_t;

// 定义OTA状态
typedef enum {
    OTA_STATE_IDLE, // 空闲状态，等待用户触发
    OTA_STATE_CHECKING, // 检查状态，等待检查升级
    OTA_STATE_DOWNLOADING,  // 下载状态，等待下载升级包
    OTA_STATE_VERIFYING,    // 验证状态，等待验证升级包
    OTA_STATE_READY,        // 准备就绪状态，等待升级
    OTA_STATE_UPDATING,     // 升级状态，正在应用升级包
    OTA_STATE_SUCCESS,      // 成功升级状态
    OTA_STATE_FAILED        // 失败升级状态
} ota_state_t;

// 定义状态机上下文结构（与 ota_framework.h 中 ota_context_t 保持一致）
typedef struct {
    ota_state_t state;  // 当前状态
    uint32_t total_size;    // 总升级包大小
    uint32_t downloaded_size;   // 已下载大小
    uint8_t progress;        //进度 0-100
    char error_msg[64];      // 错误信息缓冲区
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

// 前向声明动作函数（实现位于 table_driven_fsm.c）
void action_start_check(ota_context_t *ctx);
void action_fetch_manifest(ota_context_t *ctx);
void action_start_download(ota_context_t *ctx);
void action_start_verify(ota_context_t *ctx);
void action_prepare_update(ota_context_t *ctx);
void action_start_updating(ota_context_t *ctx);
void action_update_success(ota_context_t *ctx);
void action_update_failed(ota_context_t *ctx);
void action_no_update(ota_context_t *ctx);

// 前向声明状态机处理函数
void fsm_handle_event(ota_context_t *ctx, ota_event_t event);

void ota_engine_process(ota_context_t *ctx);


// 前向声明模块管理接口（实现位于 module_manager.c，供动作函数调用）
bool module_manager_parse_manifest(const uint8_t *manifest_data, uint32_t len);
bool module_manager_is_update_available(void);

#endif