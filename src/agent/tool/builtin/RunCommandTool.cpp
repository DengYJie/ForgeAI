#include "RunCommandTool.h"
#include "ProcessToolOperation.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>

namespace agent::tool::builtin {

    RunCommandTool::RunCommandTool(std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs)
        : m_fs(std::move(fs)) {
        if (!m_fs) {
            m_fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
        }
    }

    domain::agent::ToolDefinition RunCommandTool::definition() const {
        return {
            QStringLiteral("run_command"),
            QStringLiteral("在项目工作区内异步执行外部程序或命令行工具（如 cmake, ctest, git, python 等）。支持参数结构化传递、超时控制与前后端输出截断保护。"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("program"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("要执行的程序或可执行文件名称（如 cmake, ctest, git, ninja）")}
                    }},
                    {QStringLiteral("args"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("array")},
                        {QStringLiteral("description"), QStringLiteral("传递给程序的命令行参数列表")},
                        {QStringLiteral("items"), QJsonObject{
                            {QStringLiteral("type"), QStringLiteral("string")}
                        }}
                    }},
                    {QStringLiteral("working_directory"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("工作目录（必须在项目根目录内），默认 .")}
                    }},
                    {QStringLiteral("timeout_ms"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("integer")},
                        {QStringLiteral("minimum"), 1000},
                        {QStringLiteral("maximum"), 600000},
                        {QStringLiteral("description"), QStringLiteral("执行超时毫秒数，默认 30000 (30秒)")}
                    }}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("program")}}
            }
        };
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

        // 动态检测破坏性操作
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
        int timeoutMs = args.value(QStringLiteral("timeout_ms")).toInt(context.timeoutMs > 0 ? context.timeoutMs : 30000);
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

        return std::make_unique<ProcessToolOperation>(
            call.id,
            program,
            argList,
            workingPath,
            timeoutMs
        );
    }

} // namespace agent::tool::builtin
