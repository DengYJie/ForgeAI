#pragma once

#include <QString>
#include <QStringList>
#include <QUuid>

namespace domain::agent::task {

    /**
     * @brief 进程启动模式
     */
    enum class ProcessLaunchMode {
        DirectProcess,      ///< 直接执行底层二进制可执行文件
        ShellCommand        ///< 包装在指定终端 Shell 中执行命令字符串
    };

    /**
     * @brief 进程任务启动规格说明
     */
    struct ProcessTaskSpec {
        ProcessLaunchMode launchMode = ProcessLaunchMode::DirectProcess; ///< 启动模式

        // DirectProcess 模式专属字段
        QString program;                    ///< 可执行程序名称或路径
        QStringList arguments;              ///< 结构化命令行参数列表

        // ShellCommand 模式专属字段
        QString command;                    ///< 待执行的 Shell 命令行字符串
        QString shellProfileId;             ///< 指定使用的 ShellProfile ID（为空时使用默认 Shell）

        // 通用执行配置
        QString workingDirectory;           ///< 工作目录绝对路径
        int timeoutMs = 600000;             ///< 执行超时上限（毫秒，默认 10 分钟）
        bool background = false;            ///< 是否作为后台常驻任务启动
        QString outputEncoding = QStringLiteral("utf-8"); ///< 标准输出/错误文本解码编码 (utf-8 / system / gb18030 / shift-jis 等)

        // 属主与上下文元数据（用于安全隔离与级联清理）
        QUuid runId;                        ///< 所属 Agent Run 会话 ID
        QUuid projectId;                    ///< 所属工作区项目 ID
        QString workspaceRoot;              ///< 所属项目根路径
    };

} // namespace domain::agent::task
