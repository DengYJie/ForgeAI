#include "ProcessTaskRuntime.h"
#include "services/process/ShellService.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QUuid>
#include <QPointer>
#include <algorithm>

namespace agent::task {

    ProcessTaskRuntime::ProcessTaskRuntime(
        std::shared_ptr<application::ports::IShellService> shellService,
        QObject* parent
    ) : QObject(parent),
        m_shellService(std::move(shellService)) {
        if (!m_shellService) {
            m_shellService = std::make_shared<services::process::ShellService>();
        }
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

        auto task = std::make_shared<ProcessTask>(taskId, effectiveSpec, m_shellService, this);
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

namespace {
    class TaskWaitHandle : public application::ports::IWaitHandle {
    public:
        TaskWaitHandle(QPointer<QTimer> timer, std::shared_ptr<std::atomic<bool>> fired)
            : m_timer(timer), m_fired(std::move(fired)) {}

        ~TaskWaitHandle() override {
            cancel();
        }

        void cancel() override {
            if (m_fired && !m_fired->exchange(true)) {
                if (m_timer) {
                    m_timer->stop();
                    m_timer->deleteLater();
                }
            }
        }

        bool isCancelled() const override {
            return !m_fired || m_fired->load();
        }

    private:
        QPointer<QTimer> m_timer;
        std::shared_ptr<std::atomic<bool>> m_fired;
    };
} // namespace

    int ProcessTaskRuntime::cleanupFinishedTasks(qint64 maxAgeMs) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        int count = 0;
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_tasks.begin(); it != m_tasks.end();) {
            auto snap = it.value()->snapshot();
            const bool isFinished = (snap.state == domain::agent::task::ProcessTaskState::Completed ||
                                     snap.state == domain::agent::task::ProcessTaskState::Failed ||
                                     snap.state == domain::agent::task::ProcessTaskState::TimedOut ||
                                     snap.state == domain::agent::task::ProcessTaskState::Cancelled ||
                                     snap.state == domain::agent::task::ProcessTaskState::Crashed);
            if (isFinished) {
                const qint64 finishTime = snap.finishedAtMs > 0 ? snap.finishedAtMs : snap.startedAtMs;
                if (now - finishTime >= maxAgeMs) {
                    it = m_tasks.erase(it);
                    ++count;
                    continue;
                }
            }
            ++it;
        }
        return count;
    }

    int ProcessTaskRuntime::cleanupTasksForRun(const QUuid& runId) {
        if (runId.isNull()) return 0;
        int count = 0;
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_tasks.begin(); it != m_tasks.end();) {
            if (it.value()->spec().runId == runId) {
                auto snap = it.value()->snapshot();
                const bool isFinished = (snap.state == domain::agent::task::ProcessTaskState::Completed ||
                                         snap.state == domain::agent::task::ProcessTaskState::Failed ||
                                         snap.state == domain::agent::task::ProcessTaskState::TimedOut ||
                                         snap.state == domain::agent::task::ProcessTaskState::Cancelled ||
                                         snap.state == domain::agent::task::ProcessTaskState::Crashed);
                if (isFinished) {
                    it = m_tasks.erase(it);
                    ++count;
                    continue;
                }
            }
            ++it;
        }
        return count;
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

    std::shared_ptr<application::ports::IWaitHandle> ProcessTaskRuntime::waitForUpdateAsync(
        const QString& taskId,
        quint64 stdoutCursor,
        quint64 stderrCursor,
        int waitMs,
        std::function<void()> callback
    ) {
        auto fired = std::make_shared<std::atomic<bool>>(false);
        if (!callback) {
            fired->store(true);
            return std::make_shared<TaskWaitHandle>(nullptr, fired);
        }

        std::shared_ptr<ProcessTask> task;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_tasks.find(taskId);
            if (it != m_tasks.end()) {
                task = it.value();
            }
        }

        if (!task || waitMs <= 0) {
            auto timer = new QTimer(this);
            timer->setSingleShot(true);
            auto triggerOnce = [fired, timer, callback]() {
                if (!fired->exchange(true)) {
                    timer->stop();
                    timer->deleteLater();
                    callback();
                }
            };
            QObject::connect(timer, &QTimer::timeout, this, triggerOnce);
            timer->start(0);
            return std::make_shared<TaskWaitHandle>(timer, fired);
        }

        auto snap = task->snapshot();
        const bool isFinished = (snap.state == domain::agent::task::ProcessTaskState::Completed ||
                                 snap.state == domain::agent::task::ProcessTaskState::Failed ||
                                 snap.state == domain::agent::task::ProcessTaskState::TimedOut ||
                                 snap.state == domain::agent::task::ProcessTaskState::Cancelled ||
                                 snap.state == domain::agent::task::ProcessTaskState::Crashed);

        // 如果已有新数据或已完成，异步立即触发回调
        if (isFinished || snap.stdoutTotalBytes > stdoutCursor || snap.stderrTotalBytes > stderrCursor) {
            auto timer = new QTimer(this);
            timer->setSingleShot(true);
            auto triggerOnce = [fired, timer, callback]() {
                if (!fired->exchange(true)) {
                    timer->stop();
                    timer->deleteLater();
                    callback();
                }
            };
            QObject::connect(timer, &QTimer::timeout, this, triggerOnce);
            timer->start(0);
            return std::make_shared<TaskWaitHandle>(timer, fired);
        }

        // 构造一次性唤醒触发器
        auto timer = new QTimer(this);
        timer->setSingleShot(true);

        auto triggerOnce = [fired, timer, callback]() {
            if (!fired->exchange(true)) {
                timer->stop();
                timer->deleteLater();
                callback();
            }
        };

        QObject::connect(timer, &QTimer::timeout, this, triggerOnce);
        QObject::connect(task.get(), &ProcessTask::outputAppended, timer, [triggerOnce](const QString&) {
            triggerOnce();
        });
        QObject::connect(task.get(), &ProcessTask::finished, timer, [triggerOnce](const QString&, domain::agent::task::ProcessTaskState, int) {
            triggerOnce();
        });

        int clampedWaitMs = std::clamp(waitMs, 50, 5000);
        timer->start(clampedWaitMs);

        return std::make_shared<TaskWaitHandle>(timer, fired);
    }

    std::shared_ptr<ProcessTask> ProcessTaskRuntime::findTask(const QString& taskId) const {
        return getTask(taskId);
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
