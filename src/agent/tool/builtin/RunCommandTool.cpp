#include "RunCommandTool.h"
#include "agent/task/ProcessTaskRuntime.h"
#include "services/process/ShellService.h"
#include "services/process/ShellCommandRiskAnalyzer.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QPointer>
#include <algorithm>

namespace agent::tool::builtin {

    namespace {

        class RunCommandForegroundOperation : public application::ports::IToolOperation {
            Q_OBJECT
        public:
            RunCommandForegroundOperation(
                QString operationId,
                QString taskId,
                QString shellId,
                std::shared_ptr<application::ports::IProcessTaskRuntime> runtime,
                QUuid callerRunId,
                QObject* parent = nullptr
            ) : application::ports::IToolOperation(parent),
                m_operationId(std::move(operationId)),
                m_taskId(std::move(taskId)),
                m_shellId(std::move(shellId)),
                m_runtime(std::move(runtime)),
                m_callerRunId(callerRunId) {
            }

            ~RunCommandForegroundOperation() override {
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

                checkOrPoll();
            }

            void cancel() override {
                if (m_waitHandle) {
                    m_waitHandle->cancel();
                    m_waitHandle.reset();
                }

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
                    rootObj[QStringLiteral("encoding")] = delta.encoding;
                    rootObj[QStringLiteral("decode_error")] = delta.decodeError;
                    if (!m_shellId.isEmpty()) {
                        rootObj[QStringLiteral("shell")] = m_shellId;
                    }
                    if (!delta.exitError.isEmpty()) {
                        rootObj[QStringLiteral("error_message")] = delta.exitError;
                    }

                    QJsonObject meta;
                    meta[QStringLiteral("shell")] = m_shellId;
                    meta[QStringLiteral("exit_code")] = exitCode;
                    meta[QStringLiteral("duration_ms")] = delta.durationMs;
                    meta[QStringLiteral("stdout_bytes")] = static_cast<qint64>(snap.stdoutTotalBytes);
                    meta[QStringLiteral("stderr_bytes")] = static_cast<qint64>(snap.stderrTotalBytes);
                    meta[QStringLiteral("encoding")] = delta.encoding;
                    meta[QStringLiteral("decode_error")] = delta.decodeError;

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

                // 尚未完成，使用弱引用与 WaitHandle 防护 UAF
                QPointer<RunCommandForegroundOperation> weakThis(this);
                m_waitHandle = m_runtime->waitForUpdateAsync(m_taskId, snap.stdoutTotalBytes, snap.stderrTotalBytes, 500, [weakThis]() {
                    if (!weakThis) return;
                    weakThis->checkOrPoll();
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
            QString m_shellId;
            std::shared_ptr<application::ports::IProcessTaskRuntime> m_runtime;
            QUuid m_callerRunId;
            std::shared_ptr<application::ports::IWaitHandle> m_waitHandle;
            application::ports::ToolOperationState m_state = application::ports::ToolOperationState::Created;
            bool m_finishedEmitted = false;
        };

    } // namespace

    RunCommandTool::RunCommandTool(std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs)
        : RunCommandTool(nullptr, nullptr, std::move(fs)) {
    }

    RunCommandTool::RunCommandTool(
        std::shared_ptr<application::ports::IProcessTaskRuntime> taskRuntime,
        std::shared_ptr<application::ports::IShellService> shellService,
        std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs
    ) : m_taskRuntime(std::move(taskRuntime)),
        m_shellService(std::move(shellService)),
        m_fs(std::move(fs)) {
        if (!m_fs) {
            m_fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
        }
        if (!m_shellService) {
            m_shellService = std::make_shared<services::process::ShellService>();
        }
        if (!m_taskRuntime) {
            m_taskRuntime = std::make_shared<agent::task::ProcessTaskRuntime>(m_shellService);
        }
    }

    domain::agent::ToolDefinition RunCommandTool::definition() const {
        return {
            QStringLiteral("run_command"),
            QStringLiteral("在当前项目工作区路径下的终端 Shell 中执行命令（支持 PowerShell、Bash、Cmd 等平台默认终端脚本与复杂组合命令）。注意：工作目录被限制在工作区根路径下，进程自身拥有 ForgeAI 操作系统用户的权限。支持前台等待与 background=true 后台常驻模式（配合 check_task 增量观测）。"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("command"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("要执行的完整 Shell 命令行字符串（支持管道、&& 复合命令及终端脚本语法）")}
                    }},
                    {QStringLiteral("working_directory"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("default"), QStringLiteral(".")},
                        {QStringLiteral("description"), QStringLiteral("工作目录（相对于项目根目录），默认 .")}
                    }},
                    {QStringLiteral("background"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("boolean")},
                        {QStringLiteral("default"), false},
                        {QStringLiteral("description"), QStringLiteral("是否作为后台常驻任务启动（设为 true 将立即返回 task_id，后续通过 check_task 查询日志）")}
                    }},
                    {QStringLiteral("output_encoding"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("default"), QStringLiteral("utf-8")},
                        {QStringLiteral("description"), QStringLiteral("标准输出/错误流解码编码，如 'utf-8', 'system', 'gb18030', 'shift-jis', 'windows-1252'")}
                    }},
                    {QStringLiteral("timeout_ms"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("integer")},
                        {QStringLiteral("minimum"), 1000},
                        {QStringLiteral("maximum"), 3600000},
                        {QStringLiteral("description"), QStringLiteral("执行超时毫秒数（1000 ~ 3600000），前台默认 30000，后台默认 600000")}
                    }}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("command")}}
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
        const QString command = args.value(QStringLiteral("command")).toString().trimmed();

        QList<domain::agent::ToolPermission> perms;
        perms.append(domain::agent::ToolPermission{
            domain::agent::ToolPermissionType::ProcessExecute,
            QStringLiteral("在终端 Shell 中执行命令: %1").arg(command.length() > 80 ? command.left(77) + QStringLiteral("...") : command)
        });

        services::process::ShellCommandRiskAnalyzer analyzer;
        const auto risk = analyzer.analyze(command);
        if (risk.destructive) {
            perms.append(domain::agent::ToolPermission{
                domain::agent::ToolPermissionType::DestructiveOperation,
                QStringLiteral("高危/破坏性操作风险提示: %1 (%2)").arg(risk.reason, command.left(60))
            });
        }

        return perms;
    }

    std::unique_ptr<application::ports::IToolOperation> RunCommandTool::execute(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
        const QJsonObject args = QJsonDocument::fromJson(call.arguments.toUtf8()).object();
        const QString command = args.value(QStringLiteral("command")).toString().trimmed();

        const QString relWorkingDir = args.value(QStringLiteral("working_directory")).toString(QStringLiteral("."));
        const bool isBackground = args.value(QStringLiteral("background")).toBool(false);
        const QString outputEncoding = args.value(QStringLiteral("output_encoding")).toString(QStringLiteral("utf-8"));
        const int defaultTimeout = isBackground ? 600000 : (context.timeoutMs > 0 ? context.timeoutMs : 30000);
        int timeoutMs = args.value(QStringLiteral("timeout_ms")).toInt(defaultTimeout);
        timeoutMs = std::clamp(timeoutMs, 1000, 3600000);

        if (command.isEmpty()) {
            return std::make_unique<application::ports::ImmediateToolOperation>(
                call.id,
                [call]() -> domain::agent::ToolResult {
                    return {call.id, QStringLiteral("缺少 command 参数"), true, QStringLiteral("MissingParameter"), {}};
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

        const auto shellOpt = m_shellService ? m_shellService->defaultShell() : std::nullopt;
        if (!shellOpt.has_value()) {
            return std::make_unique<application::ports::ImmediateToolOperation>(
                call.id,
                [call]() -> domain::agent::ToolResult {
                    return {call.id, QStringLiteral("系统未配置或未找到可用的 Shell 终端环境"), true, QStringLiteral("ShellNotConfigured"), {}};
                }
            );
        }

        domain::agent::task::ProcessTaskSpec spec;
        spec.launchMode = domain::agent::task::ProcessLaunchMode::ShellCommand;
        spec.command = command;
        spec.shellProfileId = shellOpt->id;
        spec.workingDirectory = workingPath;
        spec.timeoutMs = timeoutMs;
        spec.background = isBackground;
        spec.outputEncoding = outputEncoding;
        spec.runId = context.runId;
        spec.projectId = context.projectId;
        spec.workspaceRoot = context.workspaceRoot;

        const QString taskId = m_taskRuntime->start(spec, context);

        // 后台模式：立即返回真实快照状态 (starting / running / failed 等)、task_id 与 pid
        if (isBackground) {
            auto snapOpt = m_taskRuntime->snapshot(taskId);
            const qint64 pid = snapOpt.has_value() ? snapOpt->pid : 0;
            const QString stateStr = snapOpt.has_value() ?
                (snapOpt->state == domain::agent::task::ProcessTaskState::Running || snapOpt->state == domain::agent::task::ProcessTaskState::Starting ? QStringLiteral("running") :
                (snapOpt->state == domain::agent::task::ProcessTaskState::Completed ? QStringLiteral("completed") :
                (snapOpt->state == domain::agent::task::ProcessTaskState::Failed ? QStringLiteral("failed") : QStringLiteral("running"))))
                : QStringLiteral("running");

            QJsonObject rootObj;
            rootObj[QStringLiteral("task_id")] = taskId;
            rootObj[QStringLiteral("status")] = stateStr;
            rootObj[QStringLiteral("pid")] = pid;
            rootObj[QStringLiteral("command")] = command;
            rootObj[QStringLiteral("shell")] = shellOpt->id;
            rootObj[QStringLiteral("working_directory")] = workingPath;
            rootObj[QStringLiteral("encoding")] = spec.outputEncoding;

            QJsonObject meta;
            meta[QStringLiteral("task_id")] = taskId;
            meta[QStringLiteral("pid")] = pid;
            meta[QStringLiteral("shell")] = shellOpt->id;
            meta[QStringLiteral("background")] = true;
            meta[QStringLiteral("encoding")] = spec.outputEncoding;

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
            shellOpt->id,
            m_taskRuntime,
            context.runId
        );
    }

} // namespace agent::tool::builtin

#include "RunCommandTool.moc"
