#include "CheckTaskTool.h"
#include "agent/task/ProcessTaskRuntime.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QPointer>
#include <algorithm>

namespace agent::tool::builtin {

    namespace {

        class CheckTaskOperation : public application::ports::IToolOperation {
            Q_OBJECT
        public:
            CheckTaskOperation(
                QString operationId,
                QString taskId,
                quint64 stdoutCursor,
                quint64 stderrCursor,
                int maxOutputBytes,
                int waitMs,
                std::shared_ptr<application::ports::IProcessTaskRuntime> runtime,
                QUuid callerRunId,
                QObject* parent = nullptr
            ) : application::ports::IToolOperation(parent),
                m_operationId(std::move(operationId)),
                m_taskId(std::move(taskId)),
                m_stdoutCursor(stdoutCursor),
                m_stderrCursor(stderrCursor),
                m_maxOutputBytes(maxOutputBytes),
                m_waitMs(waitMs),
                m_runtime(std::move(runtime)),
                m_callerRunId(callerRunId) {
            }

            ~CheckTaskOperation() override {
                if (m_waitHandle) {
                    m_waitHandle->cancel();
                    m_waitHandle.reset();
                }
            }

            QString operationId() const override { return m_operationId; }
            application::ports::ToolOperationState state() const override { return m_state; }

            void start() override {
                if (m_state != application::ports::ToolOperationState::Created) return;
                m_state = application::ports::ToolOperationState::Running;

                if (m_waitMs > 0 && m_runtime) {
                    QPointer<CheckTaskOperation> weakThis(this);
                    m_waitHandle = m_runtime->waitForUpdateAsync(m_taskId, m_stdoutCursor, m_stderrCursor, m_waitMs, [weakThis]() {
                        if (!weakThis) return;
                        weakThis->completeSnapshot();
                    });
                } else {
                    completeSnapshot();
                }
            }

            void cancel() override {
                if (m_waitHandle) {
                    m_waitHandle->cancel();
                    m_waitHandle.reset();
                }

                if (m_state == application::ports::ToolOperationState::Created ||
                    m_state == application::ports::ToolOperationState::Running) {
                    m_state = application::ports::ToolOperationState::Cancelled;
                    if (!m_finishedEmitted) {
                        m_finishedEmitted = true;
                        emit finished(domain::agent::ToolResult{
                            m_operationId,
                            QStringLiteral("check_task 查询已被取消"),
                            true,
                            QStringLiteral("Cancelled"),
                            {}
                        });
                    }
                }
            }

        private:
            void completeSnapshot() {
                if (m_finishedEmitted || m_state == application::ports::ToolOperationState::Cancelled) return;
                m_finishedEmitted = true;

                if (!m_runtime) {
                    m_state = application::ports::ToolOperationState::Failed;
                    emit finished(domain::agent::ToolResult{
                        m_operationId,
                        QStringLiteral("ProcessTaskRuntime 不可用"),
                        true,
                        QStringLiteral("RuntimeUnavailable"),
                        {}
                    });
                    return;
                }

                auto delta = m_runtime->readDelta(m_taskId, m_stdoutCursor, m_stderrCursor, m_maxOutputBytes, m_callerRunId);

                if (delta.exitError == QStringLiteral("TaskNotFound")) {
                    m_state = application::ports::ToolOperationState::Failed;
                    emit finished(domain::agent::ToolResult{
                        m_operationId,
                        QStringLiteral("未找到 ID 为 '%1' 的进程任务，可能已被清理或从未创建。").arg(m_taskId),
                        true,
                        QStringLiteral("TaskNotFound"),
                        {}
                    });
                    return;
                }

                if (delta.exitError == QStringLiteral("TaskNotOwnedByRun")) {
                    m_state = application::ports::ToolOperationState::Failed;
                    emit finished(domain::agent::ToolResult{
                        m_operationId,
                        QStringLiteral("出于安全原因，当前会话无权访问其他会话派生的任务 '%1'。").arg(m_taskId),
                        true,
                        QStringLiteral("TaskNotOwnedByRun"),
                        {}
                    });
                    return;
                }

                QJsonObject rootObj;
                rootObj[QStringLiteral("task_id")] = delta.taskId;
                rootObj[QStringLiteral("status")] = (delta.state == domain::agent::task::ProcessTaskState::Starting) ? QStringLiteral("starting") :
                    ((delta.state == domain::agent::task::ProcessTaskState::Running) ? QStringLiteral("running") :
                    ((delta.state == domain::agent::task::ProcessTaskState::Completed) ? QStringLiteral("completed") :
                    ((delta.state == domain::agent::task::ProcessTaskState::TimedOut) ? QStringLiteral("timed_out") :
                    ((delta.state == domain::agent::task::ProcessTaskState::Cancelled) ? QStringLiteral("cancelled") :
                    ((delta.state == domain::agent::task::ProcessTaskState::Crashed) ? QStringLiteral("crashed") : QStringLiteral("failed"))))));

                rootObj[QStringLiteral("finished")] = delta.finished;
                if (delta.exitCode.has_value()) {
                    rootObj[QStringLiteral("exit_code")] = delta.exitCode.value();
                }

                rootObj[QStringLiteral("stdout")] = delta.stdoutDelta;
                rootObj[QStringLiteral("stderr")] = delta.stderrDelta;
                rootObj[QStringLiteral("stdout_cursor")] = static_cast<qint64>(delta.nextStdoutCursor);
                rootObj[QStringLiteral("stderr_cursor")] = static_cast<qint64>(delta.nextStderrCursor);
                rootObj[QStringLiteral("encoding")] = delta.encoding;
                rootObj[QStringLiteral("decode_error")] = delta.decodeError;

                if (delta.stdoutCursorLost) {
                    rootObj[QStringLiteral("stdout_cursor_lost")] = true;
                    rootObj[QStringLiteral("stdout_available_from")] = static_cast<qint64>(delta.stdoutAvailableFrom);
                }
                if (delta.stderrCursorLost) {
                    rootObj[QStringLiteral("stderr_cursor_lost")] = true;
                    rootObj[QStringLiteral("stderr_available_from")] = static_cast<qint64>(delta.stderrAvailableFrom);
                }

                rootObj[QStringLiteral("duration_ms")] = delta.durationMs;
                if (!delta.exitError.isEmpty()) {
                    rootObj[QStringLiteral("error_message")] = delta.exitError;
                }

                QJsonObject meta;
                meta[QStringLiteral("task_id")] = delta.taskId;
                meta[QStringLiteral("finished")] = delta.finished;
                meta[QStringLiteral("stdout_cursor")] = static_cast<qint64>(delta.nextStdoutCursor);
                meta[QStringLiteral("stderr_cursor")] = static_cast<qint64>(delta.nextStderrCursor);
                meta[QStringLiteral("encoding")] = delta.encoding;
                meta[QStringLiteral("decode_error")] = delta.decodeError;

                const QString jsonText = QString::fromUtf8(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));

                m_state = application::ports::ToolOperationState::Completed;
                emit finished(domain::agent::ToolResult{
                    m_operationId,
                    jsonText,
                    false,
                    QString(),
                    meta
                });
            }

            QString m_operationId;
            QString m_taskId;
            quint64 m_stdoutCursor = 0;
            quint64 m_stderrCursor = 0;
            int m_maxOutputBytes = 32768;
            int m_waitMs = 0;
            std::shared_ptr<application::ports::IProcessTaskRuntime> m_runtime;
            QUuid m_callerRunId;
            std::shared_ptr<application::ports::IWaitHandle> m_waitHandle;

            application::ports::ToolOperationState m_state = application::ports::ToolOperationState::Created;
            bool m_finishedEmitted = false;
        };

    } // namespace

    CheckTaskTool::CheckTaskTool(std::shared_ptr<application::ports::IProcessTaskRuntime> taskRuntime)
        : m_taskRuntime(std::move(taskRuntime)) {
        if (!m_taskRuntime) {
            m_taskRuntime = std::make_shared<agent::task::ProcessTaskRuntime>();
        }
    }

    domain::agent::ToolDefinition CheckTaskTool::definition() const {
        return {
            QStringLiteral("check_task"),
            QStringLiteral("查询后台运行的进程任务状态与增量输出日志（基于单调递增绝对字节游标），支持 wait_ms 异步长轮询。"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("task_id"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("要查询的目标任务 ID (如 task_...)")}
                    }},
                    {QStringLiteral("stdout_cursor"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("integer")},
                        {QStringLiteral("minimum"), 0},
                        {QStringLiteral("default"), 0},
                        {QStringLiteral("description"), QStringLiteral("上次已消费的 stdout 绝对字节游标位置，默认 0")}
                    }},
                    {QStringLiteral("stderr_cursor"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("integer")},
                        {QStringLiteral("minimum"), 0},
                        {QStringLiteral("default"), 0},
                        {QStringLiteral("description"), QStringLiteral("上次已消费的 stderr 绝对字节游标位置，默认 0")}
                    }},
                    {QStringLiteral("max_output_bytes"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("integer")},
                        {QStringLiteral("minimum"), 1},
                        {QStringLiteral("maximum"), 1048576},
                        {QStringLiteral("default"), 32768},
                        {QStringLiteral("description"), QStringLiteral("本次增量读取的最大字节数限制（1 ~ 1048576），默认 32768 (32KB)")}
                    }},
                    {QStringLiteral("wait_ms"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("integer")},
                        {QStringLiteral("minimum"), 0},
                        {QStringLiteral("maximum"), 5000},
                        {QStringLiteral("default"), 0},
                        {QStringLiteral("description"), QStringLiteral("异步长轮询等待毫秒数（0~5000ms），在有新输出或任务结束时立即返回，默认 0")}
                    }}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("task_id")}}
            }
        };
    }

    application::ports::ToolExecutionTraits CheckTaskTool::traits() const {
        application::ports::ToolExecutionTraits t;
        t.threadSafe = false;
        t.parallelizable = true;
        t.idempotent = true;
        t.concurrencyKey = QString();
        return t;
    }

    QList<domain::agent::ToolPermission> CheckTaskTool::permissions(const domain::agent::ToolCall&) const {
        return {{
            domain::agent::ToolPermissionType::ReadOnly,
            QStringLiteral("查询后台进程任务状态与增量日志")
        }};
    }

    std::unique_ptr<application::ports::IToolOperation> CheckTaskTool::execute(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
        const QJsonObject args = QJsonDocument::fromJson(call.arguments.toUtf8()).object();
        const QString taskId = args.value(QStringLiteral("task_id")).toString().trimmed();
        const quint64 stdoutCursor = static_cast<quint64>(std::max(0LL, static_cast<long long>(args.value(QStringLiteral("stdout_cursor")).toInteger(0))));
        const quint64 stderrCursor = static_cast<quint64>(std::max(0LL, static_cast<long long>(args.value(QStringLiteral("stderr_cursor")).toInteger(0))));
        int maxOutputBytes = args.value(QStringLiteral("max_output_bytes")).toInt(32768);
        int waitMs = args.value(QStringLiteral("wait_ms")).toInt(0);

        maxOutputBytes = std::clamp(maxOutputBytes, 1, 1048576);
        waitMs = std::clamp(waitMs, 0, 5000);

        if (taskId.isEmpty()) {
            return std::make_unique<application::ports::ImmediateToolOperation>(
                call.id,
                [call]() -> domain::agent::ToolResult {
                    return {call.id, QStringLiteral("缺少 task_id 参数"), true, QStringLiteral("MissingParameter"), {}};
                }
            );
        }

        return std::make_unique<CheckTaskOperation>(
            call.id,
            taskId,
            stdoutCursor,
            stderrCursor,
            maxOutputBytes,
            waitMs,
            m_taskRuntime,
            context.runId
        );
    }

} // namespace agent::tool::builtin

#include "CheckTaskTool.moc"
