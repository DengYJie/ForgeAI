#pragma once

#include <QObject>
#include <QHash>
#include <memory>
#include <mutex>
#include "application/ports/IProcessTaskRuntime.h"
#include "application/ports/IShellService.h"
#include "ProcessTask.h"

namespace agent::task {

    /**
     * @brief 进程任务运行时核心管理实现
     * @details 负责操作系统子进程任务的创建、生命周期管理、增量读取与属主隔离。
     */
    class ProcessTaskRuntime : public QObject, public application::ports::IProcessTaskRuntime {
        Q_OBJECT
    public:
        explicit ProcessTaskRuntime(
            std::shared_ptr<application::ports::IShellService> shellService = nullptr,
            QObject* parent = nullptr
        );
        ~ProcessTaskRuntime() override;

        QString start(
            const domain::agent::task::ProcessTaskSpec& spec,
            const application::ports::ToolExecutionContext& context
        ) override;

        std::optional<domain::agent::task::ProcessTaskSnapshot> snapshot(
            const QString& taskId
        ) const override;

        domain::agent::task::ProcessOutputDelta readDelta(
            const QString& taskId,
            quint64 stdoutCursor = 0,
            quint64 stderrCursor = 0,
            int maxOutputBytes = 32768,
            const QUuid& callerRunId = {}
        ) override;

        bool cancel(
            const QString& taskId,
            const QUuid& callerRunId = {}
        ) override;

        void cancelTasksForRun(const QUuid& runId) override;
        void cancelTasksForProject(const QUuid& projectId) override;
        int cleanupFinishedTasks(qint64 maxAgeMs = 10 * 60 * 1000) override;
        int cleanupTasksForRun(const QUuid& runId) override;
        void shutdown() override;

        std::shared_ptr<application::ports::IWaitHandle> waitForUpdateAsync(
            const QString& taskId,
            quint64 stdoutCursor,
            quint64 stderrCursor,
            int waitMs,
            std::function<void()> callback
        ) override;

        std::shared_ptr<ProcessTask> findTask(const QString& taskId) const;
        std::shared_ptr<ProcessTask> getTask(const QString& taskId) const;

    private:
        std::shared_ptr<application::ports::IShellService> m_shellService;
        mutable std::mutex m_mutex;
        QHash<QString, std::shared_ptr<ProcessTask>> m_tasks;
    };

} // namespace agent::task
