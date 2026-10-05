#ifndef PRINTF_H
#define PRINTF_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

/**
 * @brief 打印调试日志（自动附带源码位置，终端可直接定位）
 * @param fmt 格式化字符串
 * @param ... 可变参数
 * @note 输出格式：[级别] 文件:行号:函数 消息
 *       在 VSCode / Trae 终端中 Ctrl+点击 "file.c:123" 即可跳转到源码位置
 * @example 打印成功日志
 LOG_SUCCESS("升级成功，固件版本已更新\n");
 * @example 打印加粗的绿色文本
 LOG_INFO(BOLDGREEN "这是加粗的绿色文字\n" RESET);
 */

// ================= 日志等级控制 =================
// 0=关闭, 1=错误, 2=错误+警告, 3=错误+警告+信息(默认), 4=全部(含调试)
// 可在编译命令中覆盖：-DLOG_LEVEL=1
#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif
// ================= 源码位置定位 =================
// 1=每条日志前缀附加 (文件:行号:函数)，终端可点击跳转源码（默认）
// 0=关闭定位（发布固件/串口带宽紧张时使用）
#ifndef LOG_LOC_ENABLE
#define LOG_LOC_ENABLE 1
#endif

// 日志等级宏定义
#define LOG_ERROR(fmt, ...) LOG_IMPL(1, BOLDRED   "[ERROR]" RESET " ", fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG_IMPL(2, BOLDYELLOW "[WARN]" RESET " ", fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  LOG_IMPL(3, GREEN     "[INFO]" RESET " ", fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG_IMPL(4, CYAN      "[DEBUG]" RESET " ", fmt, ##__VA_ARGS__)
// 状态/结果类日志
#define LOG_SUCCESS(fmt, ...) LOG_INFO(GREEN "[SUCC] ✅ " fmt RESET, ##__VA_ARGS__)
#define LOG_FAILURE(fmt, ...) LOG_ERROR(RED "[FAIL] ❌ " fmt RESET, ##__VA_ARGS__)
#define LOG_SUCCESS_(fmt, ...) LOG_INFO(GREEN "[SUCC]" U_ICON_SUCCESS fmt RESET, ##__VA_ARGS__)
#define LOG_FAILURE_(fmt, ...) LOG_ERROR(RED "[FAIL]" U_ICON_FAILURE fmt RESET, ##__VA_ARGS__)
#define LOG_PRINT_HuoJian(fmt, ...) LOG_INFO(GREEN "[INFO] 🚀 " fmt RESET, ##__VA_ARGS__)

// 状态/结果类日志（图标）
#define LOG_RED_DOT(fmt, ...)     LOG_IMPL(3, BOLDRED   "[INFO] 🔴 ", fmt RESET, ##__VA_ARGS__)
#define LOG_YELLOW_DOT(fmt, ...)  LOG_IMPL(3, BOLDYELLOW "[INFO] 🟡 ", fmt RESET, ##__VA_ARGS__)
#define LOG_GREEN_DOT(fmt, ...)   LOG_IMPL(3, BOLDGREEN "[INFO] 🟢 ", fmt RESET, ##__VA_ARGS__)
#define LOG_BLUE_DOT(fmt, ...)    LOG_IMPL(3, BOLDBLUE    "[INFO] 🔵 ", fmt RESET, ##__VA_ARGS__)
#define LOG_GRAY_DOT(fmt, ...)    LOG_IMPL(3, BOLDBGRAY "[INFO] ⚪ ", fmt RESET, ##__VA_ARGS__)
#define LOG_RED_CUBE(fmt, ...)    LOG_IMPL(3, BOLDRED   "[INFO] 🟥 ", fmt RESET, ##__VA_ARGS__)
#define LOG_YELLOW_CUBE(fmt, ...) LOG_IMPL(3, BOLDYELLOW  "[INFO] 🟧 ", fmt RESET, ##__VA_ARGS__)
#define LOG_GREEN_CUBE(fmt, ...)  LOG_IMPL(3, BOLDGREEN "[INFO] 🟩 ", fmt RESET, ##__VA_ARGS__)
#define LOG_BLUE_CUBE(fmt, ...)   LOG_IMPL(3, BOLDBLUE    "[INFO] 🟦 ", fmt RESET, ##__VA_ARGS__)

//🌊	\U0001F30A	BLUE	海浪
//🚫	\U0001F6AB	RED	错误
#define ICON_WAVE        BLUE   "🌊 " RESET    // 海浪
#define ICON_ERROR       RED    "🚫 " RESET    // 错误

// 彩色圆点🔴 🟠 🟡 🟢 🔵 🟣 🟤 ⚫ ⚪
#define RED_DOT "🔴"
#define YELLOW_DOT "🟡"
#define GREEN_DOT "🟢"
#define BLUE_DOT "🔵"
#define GRAY_DOT "⚪"
// 彩色方块🟥 🟧 🟨 🟩 🟦 🟪 🟫 ⬛ ⬜
#define RED_CUBE "🟥"
#define YELLOW_CUBE "🟧"
#define GREEN_CUBE "🟩"
#define BLUE_CUBE "🟦"
#define GRAY_CUBE "🟪"
// 彩色爱心❤️ 🧡 💛 💚 💙 💜 🤎 🖤 🤍 🤎
#define HEART "❤️"
// 三角菱形🔺 🔻 🔸 🔹 🔶 🔷
// 旗帜星星🚩 🏁 ⭐ 🌟 🌠 🌝 🌚 🌞



// __FILE_NAME__（GCC 12+ / Clang 15+）直接提供源文件名；旧编译器回退到运行时截取
// 例如：__FILE__ = "module/fsm/ota_fsm.c" -> "ota_fsm.c"
#if defined(__FILE_NAME__)
#define LOG_LOC_FILE __FILE_NAME__
#else
static inline const char *log_basename(const char *path) {
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
}
#define LOG_LOC_FILE log_basename(__FILE__)
#endif

// 日志打印实现函数声明
#if LOG_LOC_ENABLE
// 位置前缀：灰色 (文件:行号:函数)，在 LOG_* 调用处展开
#define LOG_LOC_FMT  GRAY "%s:%d:%s" RESET " "
#define LOG_LOC_ARGS LOG_LOC_FILE, __LINE__, __func__

// 实际打印实现：根据日志等级和位置定位，选择是否打印
#define LOG_IMPL(level, tag, fmt, ...) \
    do { if (LOG_LEVEL >= (level)) printf(tag LOG_LOC_FMT fmt, LOG_LOC_ARGS, ##__VA_ARGS__); } while (0)
#else
#define LOG_IMPL(level, tag, fmt, ...) \
    do { if (LOG_LEVEL >= (level)) printf(tag fmt, ##__VA_ARGS__); } while (0)
#endif

// 原始打印：无级别/位置前缀，适合 \r 单行进度刷新等场景；主动 flush 保证立即输出
#define LOG_RAW(...) \
    do { printf(__VA_ARGS__); fflush(stdout); } while (0)

// 十六进制转储（受 LOG_LEVEL>=4 控制），调试固件/报文数据
#if LOG_LEVEL >= 4
#define LOG_HEX(data, len) log_hex_dump((data), (size_t)(len))
#else
#define LOG_HEX(data, len) ((void)0)
#endif

// 定义颜色, 格式: \033[显示方式;前景色2色;背景色m3
#define RESET   "\033[0m"       /* Reset */
#define GRAY    "\033[37m"      /* 灰色 */
#define BLACK   "\033[30m"      /* 黑色 */
#define RED     "\033[31m"      /* 红色 */
#define GREEN   "\033[32m"      /* 绿色 */
#define YELLOW  "\033[33m"      /* 黄色 */
#define BLUE    "\033[34m"      /* 蓝色 */
#define MAGENTA "\033[35m"      /* 紫色 */
#define CYAN    "\033[36m"      /* 青色 */
#define WHITE   "\033[37m"      /* 白色 */
#define BOLDBGRAY    "\033[1m\033[37m"      /* 粗灰 */
#define BOLDBLACK   "\033[1m\033[30m"      /* 粗黑 */
#define BOLDRED     "\033[1m\033[31m"      /* 粗红 */
#define BOLDGREEN   "\033[1m\033[32m"      /* 粗绿 */
#define BOLDYELLOW  "\033[1m\033[33m"      /* 粗黄 */
#define BOLDBLUE    "\033[1m\033[34m"      /* 粗蓝 */
#define BOLDMAGENTA "\033[1m\033[35m"      /* 粗紫 */
#define BOLDCYAN    "\033[1m\033[36m"      /* 粗青 */
#define BOLDWHITE   "\033[1m\033[37m"      /* 粗白 */

/*
状态与结果指示 (Status & Results)：
常用于函数返回值判断、测试结果输出或状态机最终状态汇报。
符号	码点	推荐配色	语义说明
✅	\u2705	GREEN	成功、通过、完成
❌	\u274C	RED	失败、错误、拒绝
⚠️	\u26A0	YELLOW	警告、注意、非致命异常
🛑	\U0001F6D1	RED	停止、阻断、致命错误
❗	\u2757	RED	强调错误、危险
ℹ️	\u2139	BLUE	提示、信息说明
❓	\u2753	YELLOW	疑问、待定
⭕	\u274C	RED	待办、未完成

*/
// 状态与结果指示宏定义 (颜色 + 符号 + 重置)
#define ICON_SUCCESS  GREEN  "✅ " RESET    // 成功、通过、完成
#define ICON_FAIL     RED    "❌ " RESET    // 失败、错误、拒绝
#define ICON_WARN     YELLOW "⚠️ " RESET    // 警告、注意、非致命异常
#define ICON_STOP     RED    "🛑 " RESET    // 停止、阻断、致命错误
#define ICON_DANGER   RED    "❗ " RESET    // 强调错误、危险
#define ICON_INFO     BLUE   "ℹ️  " RESET    // 提示、信息说明
#define ICON_QUESTION YELLOW "❓ " RESET    // 疑问、待定
#define ICON_TODO     RED    "⭕ " RESET    // 待办、未完成

#define U_ICON_SUCCESS  GREEN  "\u2705 "    // 成功、通过、完成
#define U_ICON_FAIL     RED    "\u274C "     // 失败、错误、拒绝
#define U_ICON_WARN     YELLOW "\u26A0 "     // 警告、注意、非致命异常
#define U_ICON_STOP     RED    "\U0001F6D1 "     // 停止、阻断、致命错误
#define U_ICON_DANGER   RED    "\u2757 "     // 强调错误、危险
#define U_ICON_INFO     BLUE   "\u2139 "     // 提示、信息说明

/*
运行与操作指示 (Operations & Actions)：
常用于事件触发、任务启动或流程步骤打印。
符号	码点	推荐配色	语义说明
🚀	\U0001F680	CYAN / MAGENTA	启动、部署、加速运行
🛠️	\U0001F6E0	YELLOW	配置、构建、修复
🔍	\U0001F50D	BLUE	搜索、查找匹配规则
🔄	\U0001F504	GREEN	刷新、重试、循环
⏳	\u23F3	YELLOW	等待、超时、处理中
📌	\U0001F4CC	RED	标记、锚点、关键事件
*/

// 运行与操作指示宏定义 (颜色 + 符号 + 重置)
#define ICON_START    CYAN   "🚀 " RESET    // 启动、运行
#define ICON_SEARCH   BLUE   "🔍 " RESET    // 搜索、查找
#define ICON_PIN      MAGENTA "📌 " RESET   // 标记、固定
#define ICON_QUESTION YELLOW "❓ " RESET    // 疑问、待定
#define ICON_REFRESH  GREEN  "🔄 " RESET    // 刷新、重试
#define ICON_TIMEOUT  YELLOW "⏳ " RESET    // 等待、超时
#define ICON_CONFIG   YELLOW "🛠️ " RESET    // 配置、构建、修复

#define U_ICON_START    CYAN   "\U0001F680 "     // 启动、运行
#define U_ICON_SEARCH   BLUE   "\U0001F50D "     // 搜索、查找
#define U_ICON_PIN      MAGENTA "\U0001F4CC "     // 标记、固定
#define U_ICON_QUESTION YELLOW "\U0001F6E0 "     // 疑问、待定
#define U_ICON_REFRESH  GREEN  "\U0001F504 "     // 刷新、重试
#define U_ICON_TIMEOUT  YELLOW "\u23F3 "     // 等待、超时
#define U_ICON_CONFIG   YELLOW "\U0001F6E0 "     // 配置、构建、修复
/*
数据与资源指示 (Data & Resources)：
常用于文件操作、网络请求或数据解析日志。
符号	码点	推荐配色	语义说明
📂	\U0001F4C2	BLUE	打开文件夹、目录
📄	\U0001F4C4	WHITE	文件、文档
💾	\U0001F4BE	GREEN	保存、写入成功
🔌	\U0001F50C	GREEN	连接成功、插头
✂️	\u2702	YELLOW	裁剪、删除、断开
*/
// 数据与资源指示宏定义 (颜色 + 符号 + 重置)
#define ICON_FOLDER   BLUE   "📂 " RESET    // 打开文件夹、目录
#define ICON_FILE     WHITE  "📄 " RESET    // 文件、文档
#define ICON_SAVE     GREEN  "💾 " RESET    // 保存、写入成功
#define ICON_CONNECT  GREEN  "🔌 " RESET    // 连接成功、插头
#define ICON_CUT      YELLOW "✂️ " RESET    // 裁剪、删除、断开

#define U_ICON_FOLDER   BLUE   "\U0001F4C2"     // 打开文件夹、目录
#define U_ICON_FILE     WHITE  "\U0001F4C4"     // 文件、文档
#define U_ICON_SAVE     GREEN  "\U0001F4BE"     // 保存、写入成功
#define U_ICON_CONNECT  GREEN  "\U0001F50C"     // 连接成功、插头
#define U_ICON_CUT      YELLOW "\u2702"     // 裁剪、删除、断开
/*
终端 UI 几何装饰 (Terminal UI Shapes)
这些符号没有彩色 Emoji 版本，必须依赖 ANSI 转义码上色，常用于绘制进度条、树形结构或表格边框。
符号	码点	推荐配色	语义说明
●	\u25CF	GREEN / RED	状态圆点（在线/离线）
○	\u25CB	WHITE	空心圆点（未选中/空闲）
◉	\u25C9	BLUE	目标、单选选中
▸	\u25B8	CYAN	列表项、折叠节点
▾	\u25BE	CYAN	展开节点
█	\u2588	GREEN	进度条满格
░	\u2591	WHITE	进度条背景空格
*/
// 终端 UI 几何装饰宏定义 (颜色 + 符号 + 重置)
#define ICON_DOT_ON      GREEN  "● " RESET    // 状态圆点（在线/激活）
#define ICON_DOT_OFF     WHITE  "○ " RESET    // 空心圆点（离线/空闲/未选中）
#define ICON_TARGET      BLUE   "◉ " RESET    // 目标、单选选中
#define ICON_FOLD        CYAN   "▸ " RESET    // 列表项、折叠节点
#define ICON_UNFOLD      CYAN   "▾ " RESET    // 展开节点
#define ICON_PROGRESS_F  GREEN  "█ " RESET    // 进度条满格
#define ICON_PROGRESS_E  WHITE  "░ " RESET    // 进度条背景空格

#define U_ICON_DOT_ON      GREEN  "\u25CF "    // 状态圆点（在线/激活）
#define U_ICON_DOT_OFF     WHITE  "\u25CB "    // 空心圆点（离线/空闲/未选中）
#define U_ICON_TARGET      BLUE   "\u25C9 "    // 目标、单选选中
#define U_ICON_FOLD        CYAN   "\u25B8 "    // 列表项、折叠节点
#define U_ICON_UNFOLD      CYAN   "\u25BE "    // 展开节点
#define U_ICON_PROGRESS_F  GREEN  "\u2588 "    // 进度条满格
#define U_ICON_PROGRESS_E  WHITE  "\u2591 "    // 进度条背景空格


// 十六进制转储：每行 16 字节，带 ASCII 列；受 LOG_LEVEL>=4（调试级）控制
static inline void log_hex_dump(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i += 16) {
        printf(CYAN "[DEBUG]" RESET GRAY " %04zx " RESET, i);
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < len) printf("%02X ", p[i + j]);
            else             printf("   ");
            if (j == 7)      printf(" ");
        }
        printf(GRAY "|" RESET);
        for (size_t j = 0; j < 16 && i + j < len; ++j) {
            uint8_t c = p[i + j];
            printf("%c", (c >= 0x20 && c < 0x7F) ? c : '.');
        }
        printf(GRAY "|" RESET "\n");
    }
}

#endif // PRINTF_H
