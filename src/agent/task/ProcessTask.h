#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QElapsedTimer>
#include <memory>
#include "domain/agent/task/ProcessTaskSpec.h"
#include "domain/agent/task/ProcessTaskSnapshot.h"
#include "domain/agent/task/ProcessOutputDelta.h"
#include "application/ports/IShellService.h"
#include "ProcessOutputBuffer.h"

namespace agent::task {

    /**
     * @brief 操作系统进程异步执行任务实例
     * @details 独立管理单个子进程的完整生命周期、增量输出捕获、超时看门狗与安全取消。
     */
    class ProcessTask : public QObject {
        Q_OBJECT
    public:
        ProcessTask(
            QString taskId,
            domain::agent::task::ProcessTaskSpec spec,
            std::shared_ptr<application::ports::IShellService> shellService = nullptr,
            QObject* parent = nullptr
        );

        ~ProcessTask() override;

        QString taskId() const;
        domain::agent::task::ProcessTaskState state() const;
        const domain::agent::task::ProcessTaskSpec& spec() const;

        /**
         * @brief 启动子进程并进入异步执行状态
         */
        bool start();

        /**
         * @brief 取消并强制终止子进程
         */
        void cancel();

        /**
         * @brief 获取当前任务只读快照
         */
        domain::agent::task::ProcessTaskSnapshot snapshot() const;

        /**
         * @brief 基于绝对游标读取增量日志
         */
        domain::agent::task::ProcessOutputDelta readDelta(
            quint64 stdoutCursor = 0,
            quint64 stderrCursor = 0,
            int maxOutputBytes = 32768
        ) const;

    Q_SIGNALS:
        void outputAppended(const QString& taskId);
        void stateChanged(const QString& taskId, domain::agent::task::ProcessTaskState newState);
        void finished(const QString& taskId, domain::agent::task::ProcessTaskState finalState, int exitCode);

    private Q_SLOTS:
        void onReadyReadStdout();
        void onReadyReadStderr();
        void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
        void onProcessError(QProcess::ProcessError error);
        void onTimeout();

    private:
        void setState(domain::agent::task::ProcessTaskState newState);
        void finalizeTask(domain::agent::task::ProcessTaskState finalState, int exitCode, const QString& exitError = QString());

        QString m_taskId;
        domain::agent::task::ProcessTaskSpec m_spec;
        std::shared_ptr<application::ports::IShellService> m_shellService;
        domain::agent::task::ProcessTaskState m_state = domain::agent::task::ProcessTaskState::Starting;

        QProcess* m_process = nullptr;
        QTimer* m_timeoutTimer = nullptr;
        QElapsedTimer m_elapsed;

        qint64 m_startedAtMs = 0;
        qint64 m_finishedAtMs = 0;
        qint64 m_pid = 0;
        int m_exitCode = -1;
        QString m_exitError;
        bool m_timedOut = false;
        bool m_finishedEmitted = false;

        mutable ProcessOutputBuffer m_stdoutBuffer;
        mutable ProcessOutputBuffer m_stderrBuffer;
    };

} // namespace agent::task
