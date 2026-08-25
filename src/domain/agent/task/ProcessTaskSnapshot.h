#pragma once

#include <QString>
#include <QStringList>
#include <QUuid>
#include <optional>
#include "ProcessTaskState.h"

namespace domain::agent::task {

    /**
     * @brief 进程任务运行时只读快照
     */
    struct ProcessTaskSnapshot {
        QString taskId;                             ///< 任务全局唯一 ID (格式: task_<uuid>)
        ProcessTaskState state = ProcessTaskState::Starting;
        QString program;
        QStringList arguments;
        QString workingDirectory;

        qint64 pid = 0;                             ///< 操作系统进程 PID（仅作元数据参考）
        std::optional<int> exitCode;                ///< 进程退出码（运行中为 nullopt）

        qint64 startedAtMs = 0;                     ///< 启动时间戳（毫秒）
        qint64 finishedAtMs = 0;                    ///< 完成时间戳（毫秒，未完成为 0）

        quint64 stdoutTotalBytes = 0;               ///< stdout 累计产出字节总数
        quint64 stderrTotalBytes = 0;               ///< stderr 累计产出字节总数
        bool stdoutTruncated = false;               ///< 是否曾发生历史缓冲区截断
        bool stderrTruncated = false;

        QString exitError;                          ///< 异常退出原因（如 TimedOut, Crashed, Cancelled）

        QUuid runId;
        QUuid projectId;
        QString workspaceRoot;
    };

} // namespace domain::agent::task
