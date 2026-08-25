#pragma once

#include <QString>
#include <optional>
#include "ProcessTaskState.h"

namespace domain::agent::task {

    /**
     * @brief 增量输出与游标读取结果
     */
    struct ProcessOutputDelta {
        QString taskId;                             ///< 任务 ID
        ProcessTaskState state = ProcessTaskState::Starting;
        bool finished = false;                      ///< 任务是否已结束
        std::optional<int> exitCode;                ///< 退出码

        QString stdoutDelta;                        ///< 本次游标增量 stdout 文本
        QString stderrDelta;                        ///< 本次游标增量 stderr 文本

        quint64 nextStdoutCursor = 0;               ///< 下次读取建议传入的 stdout 游标
        quint64 nextStderrCursor = 0;               ///< 下次读取建议传入的 stderr 游标

        bool stdoutCursorLost = false;              ///< 请求的游标是否已滑出内存缓冲区
        bool stderrCursorLost = false;

        quint64 stdoutAvailableFrom = 0;            ///< 当前内存中最早可读字节位置
        quint64 stderrAvailableFrom = 0;

        qint64 durationMs = 0;                      ///< 累计运行时长（毫秒）
        QString exitError;
        QString encoding = QStringLiteral("utf-8"); ///< 实际使用的解码编码
        bool decodeError = false;                   ///< 解码过程中是否遇到无法解析的损坏/非法字节
    };

} // namespace domain::agent::task
