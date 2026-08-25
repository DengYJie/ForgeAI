#include "ProcessTaskRuntime.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QUuid>
#include <QPointer>
#include <algorithm>

namespace agent::task {

    ProcessTaskRuntime::ProcessTaskRuntime(QObject* parent)
        : QObject(parent) {
    }

    ProcessTaskRuntime::~ProcessTaskRuntime() {
        shutdown();
    }

    QString ProcessTaskRuntime::start(
        const domain::agent::task::ProcessTaskSpec& spec,
        const application::ports::ToolExecutionContext& context
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);

        const QString taskId = QStringLiteral("task_") + QUuid::createUuid().toString(QUuid::WithoutBraces);

        domain::agent::task::ProcessTaskSpec effectiveSpec = spec;
        if (effectiveSpec.runId.isNull()) {
            effectiveSpec.runId = context.runId;
        }
        if (effectiveSpec.projectId.isNull()) {
            effectiveSpec.projectId = context.projectId;
        }
        if (effectiveSpec.workspaceRoot.isEmpty()) {
            effectiveSpec.workspaceRoot = context.workspaceRoot;
        }
        if (effectiveSpec.workingDirectory.isEmpty()) {
            effectiveSpec.workingDirectory = context.workspaceRoot;
        }

        auto task = std::make_shared<ProcessTask>(taskId, effectiveSpec, this);
        m_tasks.insert(taskId, task);

        task->start();
        return taskId;
    }

    std::optional<domain::agent::task::ProcessTaskSnapshot> ProcessTaskRuntime::snapshot(
        const QString& taskId
    ) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_tasks.find(taskId);
        if (it != m_tasks.end()) {
            return it.value()->snapshot();
        }
        return std::nullopt;
    }

    domain::agent::task::ProcessOutputDelta ProcessTaskRuntime::readDelta(
        const QString& taskId,
        quint64 stdoutCursor,
        quint64 stderrCursor,
        int maxOutputBytes,
        const QUuid& callerRunId
    ) {
        std::shared_ptr<ProcessTask> task;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_tasks.find(taskId);
            if (it != m_tasks.end()) {
                task = it.value();
            }
        }

        if (!task) {
            domain::agent::task::ProcessOutputDelta notFound;
            notFound.taskId = taskId;
            notFound.finished = true;
            notFound.exitError = QStringLiteral("TaskNotFound");
            return notFound;
        }

        // 属主安全隔离检查
        if (!callerRunId.isNull() && !task->spec().runId.isNull() && task->spec().runId != callerRunId) {
            domain::agent::task::ProcessOutputDelta unauthorized;
            unauthorized.taskId = taskId;
            unauthorized.finished = true;
            unauthorized.exitError = QStringLiteral("TaskNotOwnedByRun");
            return unauthorized;
        }

        return task->readDelta(stdoutCursor, stderrCursor, maxOutputBytes);
    }

    bool ProcessTaskRuntime::cancel(
        const QString& taskId,
        const QUuid& callerRunId
    ) {
        std::shared_ptr<ProcessTask> task;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_tasks.find(taskId);
            if (it != m_tasks.end()) {
                task = it.value();
            }
        }

        if (!task) return false;

        if (!callerRunId.isNull() && !task->spec().runId.isNull() && task->spec().runId != callerRunId) {
            return false;
        }

        task->cancel();
        return true;
    }

    void ProcessTaskRuntime::cancelTasksForRun(const QUuid& runId) {
        if (runId.isNull()) return;

        QList<std::shared_ptr<ProcessTask>> targetTasks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto it = m_tasks.cbegin(); it != m_tasks.cend(); ++it) {
                if (it.value()->spec().runId == runId) {
                    targetTasks.append(it.value());
                }
            }
        }

        for (const auto& t : targetTasks) {
            t->cancel();
        }
    }

    void ProcessTaskRuntime::cancelTasksForProject(const QUuid& projectId) {
        if (projectId.isNull()) return;

        QList<std::shared_ptr<ProcessTask>> targetTasks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto it = m_tasks.cbegin(); it != m_tasks.cend(); ++it) {
                if (it.value()->spec().projectId == projectId) {
                    targetTasks.append(it.value());
                }
            }
        }

        for (const auto& t : targetTasks) {
            t->cancel();
        }
    }

    void ProcessTaskRuntime::shutdown() {
        QList<std::shared_ptr<ProcessTask>> allTasks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto it = m_tasks.cbegin(); it != m_tasks.cend(); ++it) {
                allTasks.append(it.value());
            }
            m_tasks.clear();
        }

        for (const auto& t : allTasks) {
            t->cancel();
        }
    }

    void ProcessTaskRuntime::waitForUpdateAsync(
        const QString& taskId,
        quint64 stdoutCursor,
        quint64 stderrCursor,
        int waitMs,
        std::function<void()> callback
    ) {
        if (!callback) return;

        std::shared_ptr<ProcessTask> task;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_tasks.find(taskId);
            if (it != m_tasks.end()) {
                task = it.value();
            }
        }

        if (!task || waitMs <= 0) {
            QTimer::singleShot(0, this, [callback]() { callback(); });
            return;
        }

        auto snap = task->snapshot();
        const bool isFinished = (snap.state == domain::agent::task::ProcessTaskState::Completed ||
                                 snap.state == domain::agent::task::ProcessTaskState::Failed ||
                                 snap.state == domain::agent::task::ProcessTaskState::TimedOut ||
                                 snap.state == domain::agent::task::ProcessTaskState::Cancelled ||
                                 snap.state == domain::agent::task::ProcessTaskState::Crashed);

        // 如果已有新数据或已完成，立即触发回调
        if (isFinished || snap.stdoutTotalBytes > stdoutCursor || snap.stderrTotalBytes > stderrCursor) {
            QTimer::singleShot(0, this, [callback]() { callback(); });
            return;
        }

        // 构造一次性唤醒触发器
        auto fired = std::make_shared<std::atomic<bool>>(false);
        auto timer = new QTimer(this);
        timer->setSingleShot(true);

        auto triggerOnce = [fired, timer, callback]() {
            if (!fired->exchange(true)) {
                timer->stop();
                timer->deleteLater();
                callback();
            }
        };

        connect(timer, &QTimer::timeout, this, triggerOnce);
        connect(task.get(), &ProcessTask::outputAppended, timer, [triggerOnce](const QString&) {
            triggerOnce();
        });
        connect(task.get(), &ProcessTask::finished, timer, [triggerOnce](const QString&, domain::agent::task::ProcessTaskState, int) {
            triggerOnce();
        });

        int clampedWaitMs = std::clamp(waitMs, 50, 5000);
        timer->start(clampedWaitMs);
    }

    std::shared_ptr<ProcessTask> ProcessTaskRuntime::getTask(const QString& taskId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_tasks.find(taskId);
        if (it != m_tasks.end()) {
            return it.value();
        }
        return nullptr;
    }

} // namespace agent::task
