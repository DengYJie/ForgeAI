#include "RunCommandTool.h"
#include "agent/task/ProcessTaskRuntime.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>
#include <QTimer>

namespace agent::tool::builtin {

    namespace {

        class RunCommandForegroundOperation : public application::ports::IToolOperation {
            Q_OBJECT
        public:
            RunCommandForegroundOperation(
                QString operationId,
                QString taskId,
                std::shared_ptr<application::ports::IProcessTaskRuntime> runtime,
                QUuid callerRunId,
                QObject* parent = nullptr
            ) : application::ports::IToolOperation(parent),
                m_operationId(std::move(operationId)),
                m_taskId(std::move(taskId)),
                m_runtime(std::move(runtime)),
                m_callerRunId(callerRunId) {
            }

            ~RunCommandForegroundOperation() override = default;

            QString operationId() const override { return m_operationId; }
            application::ports::ToolOperationState state() const override { return m_state; }

            void start() override {
                if (m_state != application::ports::ToolOperationState::Created) return;
                m_state = application::ports::ToolOperationState::Running;

                checkOrPoll();
            }

            void cancel() override {
                if (m_state == application::ports::ToolOperationState::Created ||
                    m_state == application::ports::ToolOperationState::Running) {
                    m_state = application::ports::ToolOperationState::Cancelled;
                    if (m_runtime) {
                        m_runtime->cancel(m_taskId, m_callerRunId);
                    }
                    if (!m_finishedEmitted) {
                        m_finishedEmitted = true;
                        emit finished(domain::agent::ToolResult{
                            m_operationId,
                            QStringLiteral("命令执行已由用户取消"),
                            true,
                            QStringLiteral("Cancelled"),
                            {}
                        });
                    }
                }
            }

        private:
            void checkOrPoll() {
                if (!m_runtime || m_finishedEmitted || m_state == application::ports::ToolOperationState::Cancelled) return;

                auto snapOpt = m_runtime->snapshot(m_taskId);
                if (!snapOpt.has_value()) {
                    completeWithResult(domain::agent::ToolResult{
                        m_operationId,
                        QStringLiteral("任务丢失或未找到"),
                        true,
                        QStringLiteral("TaskNotFound"),
                        {}
                    });
                    return;
                }

                const auto& snap = snapOpt.value();
                const bool isDone = (snap.state == domain::agent::task::ProcessTaskState::Completed ||
                                     snap.state == domain::agent::task::ProcessTaskState::Failed ||
                                     snap.state == domain::agent::task::ProcessTaskState::TimedOut ||
                                     snap.state == domain::agent::task::ProcessTaskState::Cancelled ||
                                     snap.state == domain::agent::task::ProcessTaskState::Crashed);

                if (isDone) {
                    auto delta = m_runtime->readDelta(m_taskId, 0, 0, 512 * 1024, m_callerRunId);

                    QJsonObject rootObj;
                    const int exitCode = delta.exitCode.value_or(-1);
                    const bool isError = (snap.state != domain::agent::task::ProcessTaskState::Completed);

                    rootObj[QStringLiteral("status")] = (snap.state == domain::agent::task::ProcessTaskState::Completed) ? QStringLiteral("completed") :
                        (snap.state == domain::agent::task::ProcessTaskState::TimedOut ? QStringLiteral("timed_out") :
                        (snap.state == domain::agent::task::ProcessTaskState::Cancelled ? QStringLiteral("cancelled") :
                        (snap.state == domain::agent::task::ProcessTaskState::Crashed ? QStringLiteral("crashed") : QStringLiteral("failed"))));
                    rootObj[QStringLiteral("exit_code")] = exitCode;
                    rootObj[QStringLiteral("stdout")] = delta.stdoutDelta;
                    rootObj[QStringLiteral("stderr")] = delta.stderrDelta;
                    rootObj[QStringLiteral("duration_ms")] = delta.durationMs;
                    if (!delta.exitError.isEmpty()) {
                        rootObj[QStringLiteral("error_message")] = delta.exitError;
                    }

                    QJsonObject meta;
                    meta[QStringLiteral("exit_code")] = exitCode;
                    meta[QStringLiteral("duration_ms")] = delta.durationMs;
                    meta[QStringLiteral("stdout_bytes")] = static_cast<qint64>(snap.stdoutTotalBytes);
                    meta[QStringLiteral("stderr_bytes")] = static_cast<qint64>(snap.stderrTotalBytes);

                    const QString jsonText = QString::fromUtf8(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));

                    m_state = isError ? application::ports::ToolOperationState::Failed : application::ports::ToolOperationState::Completed;

                    completeWithResult(domain::agent::ToolResult{
                        m_operationId,
                        jsonText,
                        isError,
                        isError ? (snap.state == domain::agent::task::ProcessTaskState::TimedOut ? QStringLiteral("TimedOut") : QStringLiteral("CommandFailed")) : QString(),
                        meta
                    });
                    return;
                }

                // 尚未完成，使用非阻塞异步等待
                m_runtime->waitForUpdateAsync(m_taskId, snap.stdoutTotalBytes, snap.stderrTotalBytes, 500, [this]() {
                    checkOrPoll();
                });
            }

            void completeWithResult(const domain::agent::ToolResult& res) {
                if (!m_finishedEmitted) {
                    m_finishedEmitted = true;
                    emit finished(res);
                }
            }

            QString m_operationId;
            QString m_taskId;
            std::shared_ptr<application::ports::IProcessTaskRuntime> m_runtime;
            QUuid m_callerRunId;
            application::ports::ToolOperationState m_state = application::ports::ToolOperationState::Created;
            bool m_finishedEmitted = false;
        };

    } // namespace

    RunCommandTool::RunCommandTool(std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs)
        : RunCommandTool(nullptr, std::move(fs)) {
    }

    RunCommandTool::RunCommandTool(
        std::shared_ptr<application::ports::IProcessTaskRuntime> taskRuntime,
        std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs
    ) : m_taskRuntime(std::move(taskRuntime)),
        m_fs(std::move(fs)) {
        if (!m_fs) {
            m_fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
        }
        if (!m_taskRuntime) {
            m_taskRuntime = std::make_shared<agent::task::ProcessTaskRuntime>();
        }
    }

    domain::agent::ToolDefinition RunCommandTool::definition() const {
        return {
            QStringLiteral("run_command"),
            QStringLiteral("在当前项目工作区内执行外部程序或命令行工具（如 cmake, ctest, git, python 等）。支持前台同步等待与 background=true 后台常驻模式（配合 check_task 查询增量日志）。"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("program"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("要执行的程序或可执行文件名称（如 cmake, ctest, git, ninja）")}
                    }},
                    {QStringLiteral("args"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("array")},
                        {QStringLiteral("description"), QStringLiteral("传递给程序的结构化参数列表")},
                        {QStringLiteral("items"), QJsonObject{
                            {QStringLiteral("type"), QStringLiteral("string")}
                        }}
                    }},
                    {QStringLiteral("working_directory"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("default"), QStringLiteral(".")},
                        {QStringLiteral("description"), QStringLiteral("工作目录（必须在项目根目录内），默认 .")}
                    }},
                    {QStringLiteral("background"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("boolean")},
                        {QStringLiteral("default"), false},
                        {QStringLiteral("description"), QStringLiteral("是否作为后台常驻任务启动（设为 true 将立即返回 task_id，后续通过 check_task 查询日志）")}
                    }},
                    {QStringLiteral("timeout_ms"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("integer")},
                        {QStringLiteral("minimum"), 1000},
                        {QStringLiteral("maximum"), 3600000},
                        {QStringLiteral("description"), QStringLiteral("执行超时毫秒数，前台默认 30000，后台默认 600000")}
                    }}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("program")}}
            }
        };
    }

    application::ports::ToolExecutionTraits RunCommandTool::traits() const {
        application::ports::ToolExecutionTraits t;
        t.threadSafe = false;
        t.parallelizable = false;
        t.idempotent = false;
        t.concurrencyKey = QStringLiteral("workspace:run_command");
        return t;
    }

    QList<domain::agent::ToolPermission> RunCommandTool::permissions(const domain::agent::ToolCall& call) const {
        const QJsonObject args = QJsonDocument::fromJson(call.arguments.toUtf8()).object();
        const QString program = args.value(QStringLiteral("program")).toString().trimmed().toLower();
        const QJsonArray argsArray = args.value(QStringLiteral("args")).toArray();

        QStringList argList;
        for (const auto& a : argsArray) {
            argList.append(a.toString().toLower());
        }
        const QString fullCommand = program + QLatin1Char(' ') + argList.join(QLatin1Char(' '));

        bool isDestructive = false;
        if (program == QStringLiteral("rm") || program == QStringLiteral("del") ||
            program == QStringLiteral("rmdir") || program == QStringLiteral("format") ||
            program == QStringLiteral("rd")) {
            isDestructive = true;
        } else if (program == QStringLiteral("git")) {
            if (fullCommand.contains(QStringLiteral("reset --hard")) ||
                fullCommand.contains(QStringLiteral("clean -f")) ||
                fullCommand.contains(QStringLiteral("clean -df")) ||
                fullCommand.contains(QStringLiteral("checkout -f"))) {
                isDestructive = true;
            }
        }

        if (isDestructive) {
            return {{
                domain::agent::ToolPermissionType::DestructiveOperation,
                QStringLiteral("执行高危/破坏性进程命令: %1").arg(program)
            }};
        }

        return {{
            domain::agent::ToolPermissionType::ProcessExecute,
            QStringLiteral("执行外部程序命令: %1").arg(program)
        }};
    }

    std::unique_ptr<application::ports::IToolOperation> RunCommandTool::execute(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
        const QJsonObject args = QJsonDocument::fromJson(call.arguments.toUtf8()).object();
        const QString program = args.value(QStringLiteral("program")).toString().trimmed();
        const QJsonArray argsArray = args.value(QStringLiteral("args")).toArray();
        QStringList argList;
        for (const auto& a : argsArray) {
            argList.append(a.toString());
        }

        const QString relWorkingDir = args.value(QStringLiteral("working_directory")).toString(QStringLiteral("."));
        const bool isBackground = args.value(QStringLiteral("background")).toBool(false);
        const int defaultTimeout = isBackground ? 600000 : (context.timeoutMs > 0 ? context.timeoutMs : 30000);
        int timeoutMs = args.value(QStringLiteral("timeout_ms")).toInt(defaultTimeout);
        if (timeoutMs < 1000) timeoutMs = 1000;

        if (program.isEmpty()) {
            return std::make_unique<application::ports::ImmediateToolOperation>(
                call.id,
                [call]() -> domain::agent::ToolResult {
                    return {call.id, QStringLiteral("缺少 program 参数"), true, QStringLiteral("MissingParameter"), {}};
                }
            );
        }

        QString error;
        const QString workingPath = m_fs->resolveReadablePath(context.workspaceRoot, relWorkingDir, &error);
        if (workingPath.isEmpty()) {
            core::logging::LoggingService::instance().warn(core::logging::Category::AgentTool, QStringLiteral("run_command 工作目录越界被拒绝"), {
                {QStringLiteral("dir"), relWorkingDir}
            });
            return std::make_unique<application::ports::ImmediateToolOperation>(
                call.id,
                [call, error, relWorkingDir]() -> domain::agent::ToolResult {
                    return {
                        call.id,
                        QStringLiteral("出于安全原因，工作目录不能逃逸出项目根目录: ") + (error.isEmpty() ? relWorkingDir : error),
                        true,
                        QStringLiteral("WorkingDirectoryEscape"),
                        {}
                    };
                }
            );
        }

        const QDir dir(workingPath);
        if (!dir.exists()) {
            return std::make_unique<application::ports::ImmediateToolOperation>(
                call.id,
                [call, relWorkingDir]() -> domain::agent::ToolResult {
                    return {
                        call.id,
                        QStringLiteral("指定的工作目录不存在: ") + relWorkingDir,
                        true,
                        QStringLiteral("DirectoryNotFound"),
                        {}
                    };
                }
            );
        }

        domain::agent::task::ProcessTaskSpec spec;
        spec.program = program;
        spec.arguments = argList;
        spec.workingDirectory = workingPath;
        spec.timeoutMs = timeoutMs;
        spec.background = isBackground;
        spec.runId = context.runId;
        spec.projectId = context.projectId;
        spec.workspaceRoot = context.workspaceRoot;

        const QString taskId = m_taskRuntime->start(spec, context);

        // 后台模式：立即返回 task_id、状态与 pid
        if (isBackground) {
            auto snapOpt = m_taskRuntime->snapshot(taskId);
            const qint64 pid = snapOpt.has_value() ? snapOpt->pid : 0;

            QJsonObject rootObj;
            rootObj[QStringLiteral("task_id")] = taskId;
            rootObj[QStringLiteral("status")] = QStringLiteral("running");
            rootObj[QStringLiteral("pid")] = pid;
            rootObj[QStringLiteral("program")] = program;
            rootObj[QStringLiteral("working_directory")] = workingPath;

            QJsonObject meta;
            meta[QStringLiteral("task_id")] = taskId;
            meta[QStringLiteral("pid")] = pid;
            meta[QStringLiteral("background")] = true;

            const QString jsonText = QString::fromUtf8(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));

            return std::make_unique<application::ports::ImmediateToolOperation>(
                call.id,
                [call, jsonText, meta]() -> domain::agent::ToolResult {
                    return {call.id, jsonText, false, QString(), meta};
                }
            );
        }

        // 前台模式：异步等待任务执行完成并返回全量输出
        return std::make_unique<RunCommandForegroundOperation>(
            call.id,
            taskId,
            m_taskRuntime,
            context.runId
        );
    }

} // namespace agent::tool::builtin

#include "RunCommandTool.moc"
