#pragma once

namespace domain::agent::task {

    /**
     * @brief 进程任务生命周期状态
     */
    enum class ProcessTaskState {
        Starting,       ///< 正在准备与启动进程
        Running,        ///< 正在后台运行中
        Completed,      ///< 正常退出且 exitCode == 0
        Failed,         ///< 正常退出但 exitCode != 0
        TimedOut,       ///< 执行超过指定时间上限被看门狗终止
        Cancelled,      ///< 由用户或 Agent 会话主动取消终止
        Crashed         ///< 进程异常崩溃退出 (QProcess::CrashExit)
    };

} // namespace domain::agent::task
