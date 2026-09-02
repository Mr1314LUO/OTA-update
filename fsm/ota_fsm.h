#ifndef OTA_FSM_H
#define OTA_FSM_H

#include <stdint.h>
#include <stdbool.h>

// ==========================================
// 表格驱动 OTA 状态机 —— 对外接口
// 本头文件只描述状态/事件/上下文与处理函数，
// 不引入任何底层模块（固件升级/校验/压缩）的实现细节，
// 相关依赖一律下沉到 ota_fsm.c 中
// ==========================================

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

// 事件处理：查找转移表并执行动作、状态转移
void fsm_handle_event(ota_context_t *ctx, ota_event_t event);

// 周期处理：根据当前状态自动触发对应事件
void ota_engine_process(ota_context_t *ctx);

#endif // OTA_FSM_H
