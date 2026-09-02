#ifndef PRINTF_H
#define PRINTF_H

/**
 * @brief 打印调试日志
 * @param fmt 格式化字符串
 * @param ... 可变参数
 * @note 使用示例：
 * @example 打印红色文本
 LOG_DEBUG(RED "这是红色的文字\n" RESET);
 * @example 打印加粗的绿色文本
 LOG_INFO(BOLDGREEN "这是加粗的绿色文字\n" RESET);
 * @example 在同一行中混合不同颜色
 LOG_ERROR(BLUE "蓝色文字" YELLOW "和黄色文字" RESET "以及默认颜色文字\n");
 * @example 打印状态与结果指示"🚀"     // 启动、运行
 LOG_INFO(ICON_START " Motor started.\n");
 * @example 打印状态与结果指示"🔍"     // 查找、搜索
 LOG_INFO(ICON_SEARCH " Looking for transition rule...\n");
 * @example 打印状态与结果指示"⚠️"     // 警告、注意
 LOG_INFO(ICON_TIMEOUT " Timeout event occurred.\n");
 * @example// ⚠️ 的码点是 U+26A0
 LOG_INFO("警告: \u26A0\n");
 * @example// 🔍 的码点是 U+1F50D (超过4位，必须用大写 \U 并补齐8位)
 LOG_INFO("查找: \U0001F50D\n");
 */

// 调试日志等级：0=关闭, 1=错误, 2=普通, 3=调试
#define LOG_LEVEL 2
#define LOG_INFO(fmt, ...)  do { if (LOG_LEVEL >= 2) printf("[INFO]  "fmt, ##__VA_ARGS__); } while(0)
#define LOG_DEBUG(fmt, ...) do { if (LOG_LEVEL >= 3) printf("[DEBUG] "fmt, ##__VA_ARGS__); } while(0)
#define LOG_ERROR(fmt, ...) do { if (LOG_LEVEL >= 1) printf("[ERROR] "fmt, ##__VA_ARGS__); } while(0)
// #define LOG_DEBUG(fmt, ...) do { if (LOG_LEVEL >= 3) printf("[DEBUG] " BOLDYELLOW fmt, ##__VA_ARGS__ RESET); } while(0)
// #define LOG_INFO(fmt, ...)  do { if (LOG_LEVEL >= 2) printf("[INFO]  " GRAY fmt, ##__VA_ARGS__ RESET); } while(0)
// #define LOG_ERROR(fmt, ...) do { if (LOG_LEVEL >= 1) printf("[ERROR] " BOLDRED fmt, ##__VA_ARGS__ RESET); } while(0)

#define LOG_SUCCESS(fmt, ...)  do { if (LOG_LEVEL >= 2) printf("[INFO] ✅ " GREEN fmt, ##__VA_ARGS__ RESET); } while(0);
#define LOG_FAILURE(fmt, ...)  do { if (LOG_LEVEL >= 2) printf("[ERROR] ❌ " RED fmt, ##__VA_ARGS__ RESET); } while(0);

// 彩色圆点🔴 🟠 🟡 🟢 🔵 🟣 🟤 ⚫ ⚪
// 彩色方块🟥 🟧 🟨 🟩 🟦 🟪 🟫 ⬛ ⬜
// 彩色爱心❤️ 🧡 💛 💚 💙 💜 🤎 🖤 🤍
// 状态标记✅ ❌ ⚠️ ❗ ❓ ⭕ 🔘
// 三角菱形🔺 🔻 🔸 🔹 🔶 🔷
// 旗帜星星🚩 🏁 ⭐ 🌟
#define LOG_RED_DOT(fmt, ...)  do { if (LOG_LEVEL >= 2) printf("[INFO] 🔴 " RED fmt, ##__VA_ARGS__ RESET); } while(0);
#define LOG_YELLOW_DOT(fmt, ...)  do { if (LOG_LEVEL >= 2) printf("[INFO] 🟡 " YELLOW fmt, ##__VA_ARGS__ RESET); } while(0);
#define LOG_GREEN_DOT(fmt, ...)  do { if (LOG_LEVEL >= 2) printf("[INFO] 🟢 " GREEN fmt, ##__VA_ARGS__ RESET); } while(0);
#define LOG_BLUE_DOT(fmt, ...)  do { if (LOG_LEVEL >= 2) printf("[INFO] 🔵 " BLUE fmt, ##__VA_ARGS__ RESET); } while(0);
#define LOG_GRAY_DOT(fmt, ...)  do { if (LOG_LEVEL >= 2) printf("[INFO] ⚪ " GRAY fmt, ##__VA_ARGS__ RESET); } while(0);
#define LOG_RED_CUBE(fmt, ...)  do { if (LOG_LEVEL >= 2) printf("[INFO] 🟥 " RED fmt, ##__VA_ARGS__ RESET); } while(0);
#define LOG_YELLOW_CUBE(fmt, ...)  do { if (LOG_LEVEL >= 2) printf("[INFO] 🟧 " YELLOW fmt, ##__VA_ARGS__ RESET); } while(0);
#define LOG_GREEN_CUBE(fmt, ...)  do { if (LOG_LEVEL >= 2) printf("[INFO] 🟩 " GREEN fmt, ##__VA_ARGS__ RESET); } while(0);
#define LOG_BLUE_CUBE(fmt, ...)  do { if (LOG_LEVEL >= 2) printf("[INFO] 🟦 " BLUE fmt, ##__VA_ARGS__ RESET); } while(0);

// 定义颜色, 格式: \033[显示方式;前景色2色;背景色m3
#define RESET   "\033[0m"
#define GRAY    "\033[37m"      /* Gray */
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

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
*/
// 状态与结果指示宏定义 (颜色 + 符号 + 重置)
#define ICON_SUCCESS  GREEN  "✅ " RESET    // 成功、通过、完成
#define ICON_FAIL     RED    "❌ " RESET    // 失败、错误、拒绝
#define ICON_WARN     YELLOW "⚠️ " RESET    // 警告、注意、非致命异常
#define ICON_STOP     RED    "🛑 " RESET    // 停止、阻断、致命错误
#define ICON_DANGER   RED    "❗ " RESET    // 强调错误、危险
#define ICON_INFO     BLUE   "ℹ️  " RESET    // 提示、信息说明

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

#endif // PRINTF_H