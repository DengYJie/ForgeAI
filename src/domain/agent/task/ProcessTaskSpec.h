#pragma once

#include <QString>
#include <QStringList>
#include <QUuid>

namespace domain::agent::task {

    /**
     * @brief 进程任务启动规格说明
     */
    struct ProcessTaskSpec {
        QString program;                    ///< 可执行程序名称或路径
        QStringList arguments;              ///< 结构化命令行参数列表
        QString workingDirectory;           ///< 工作目录绝对路径
        int timeoutMs = 600000;             ///< 执行超时上限（毫秒，默认 10 分钟）
        bool background = false;            ///< 是否作为后台常驻任务启动

        // 属主与上下文元数据（用于安全隔离与级联清理）
        QUuid runId;                        ///< 所属 Agent Run 会话 ID
        QUuid projectId;                    ///< 所属工作区项目 ID
        QString workspaceRoot;              ///< 所属项目根路径
    };

} // namespace domain::agent::task
