#pragma once
#include <QUuid>
#include <QByteArray>
#include <QString>
#include <QDateTime>

namespace domain::agent {
    /**
     * @brief 智能体运行时执行状态快照（用于长任务断点续传、程序重启恢复）
     */
    struct RuntimeCheckpoint {
        QUuid id; ///< 快照全局唯一标识符
        QUuid turnId; ///< 关联的交互回合 ID

        QByteArray stateData; ///< 序列化后的 Runtime 内部执行堆栈或上下文状态
        QString resumeToken; ///< 外部唤醒 Token（用于 Human-in-the-loop 或等待外部事件）

        QDateTime savedAt; ///< 快照保存时间戳
    };
} // namespace domain::agent
