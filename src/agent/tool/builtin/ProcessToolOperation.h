#pragma once

#include "application/ports/IToolOperation.h"
#include <QProcess>
#include <QTimer>
#include <QElapsedTimer>
#include <QStringList>
#include <QByteArray>

namespace agent::tool::builtin {

    /**
     * @brief 异步进程执行操作类（基于非阻塞 QProcess）
     * @details 负责执行系统命令/编译构建/测试工具，支持异步取消、Watchdog 超时与大输出前后截断保护。
     */
    class ProcessToolOperation : public application::ports::IToolOperation {
        Q_OBJECT
    public:
        ProcessToolOperation(
            const QString& operationId,
            QString program,
            QStringList args,
            QString workingDirectory,
            int timeoutMs = 30000,
            QObject* parent = nullptr
        );

        ~ProcessToolOperation() override;

        QString operationId() const override;
        application::ports::ToolOperationState state() const override;
        void start() override;
        void cancel() override;

    private Q_SLOTS:
        void onReadyReadStdout();
        void onReadyReadStderr();
        void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
        void onProcessError(QProcess::ProcessError error);
        void onTimeout();

    private:
        void finalizeOperation(int exitCode, bool timedOut, const QString& errorString = QString());
        static QString sanitizeOutput(const QByteArray& raw, int maxBytes, bool* truncated);

        QString m_operationId;
        QString m_program;
        QStringList m_args;
        QString m_workingDirectory;
        int m_timeoutMs;

        application::ports::ToolOperationState m_state = application::ports::ToolOperationState::Created;
        QProcess* m_process = nullptr;
        QTimer* m_timeoutTimer = nullptr;
        QElapsedTimer m_elapsed;

        QByteArray m_stdoutBuffer;
        QByteArray m_stderrBuffer;
        int m_totalStdoutBytes = 0;
        int m_totalStderrBytes = 0;

        bool m_finishedEmitted = false;
        bool m_timedOut = false;
    };

} // namespace agent::tool::builtin
