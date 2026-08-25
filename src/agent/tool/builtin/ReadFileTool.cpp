#include "ReadFileTool.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QFile>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace agent::tool::builtin {

    ReadFileTool::ReadFileTool(std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs)
        : m_fs(std::move(fs)) {
        if (!m_fs) {
            m_fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
        }
    }

    domain::agent::ToolDefinition ReadFileTool::definition() const {
        return {
            QStringLiteral("read_file"),
            QStringLiteral("读取工作区内的 UTF-8 文本文件。路径必须相对于项目根目录。"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("path"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("相对项目根目录的文件路径")}
                    }}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("path")}}
            }
        };
    }

    std::unique_ptr<application::ports::IToolOperation> ReadFileTool::execute(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
        return std::make_unique<application::ports::ThreadedToolOperation>(
            call.id,
            [this, call, context]() {
                return executeInternal(call, context);
            },
            context.timeoutMs > 0 ? context.timeoutMs : 30000
        );
    }

    domain::agent::ToolResult ReadFileTool::executeInternal(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
        QElapsedTimer timer;
        timer.start();

        domain::agent::ToolResult result{call.id, {}, true};
        const QJsonObject args = QJsonDocument::fromJson(call.arguments.toUtf8()).object();
        const QString relativePath = args.value(QStringLiteral("path")).toString();

        if (context.cancellationToken.isCanceled()) {
            result.content = QStringLiteral("读取文件已被取消");
            return result;
        }

        if (relativePath.trimmed().isEmpty()) {
            result.content = QStringLiteral("缺少 path 参数");
            return result;
        }

        QString error;
        const QString path = m_fs->resolveReadablePath(context.workspaceRoot, relativePath, &error);
        if (path.isEmpty()) {
            core::logging::LoggingService::instance().warn(core::logging::Category::AgentTool, QStringLiteral("read_file 路径校验失败"), {
                {QStringLiteral("path"), relativePath},
                {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
            });
            result.content = error.isEmpty() ? QStringLiteral("出于安全原因，无法访问项目外的路径或文件不存在。") : error;
            return result;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            core::logging::LoggingService::instance().warn(core::logging::Category::AgentTool, QStringLiteral("read_file 无法打开文件"), {
                {QStringLiteral("path"), relativePath},
                {QStringLiteral("osError"), file.errorString()},
                {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
            });
            result.content = QStringLiteral("无法读取该文件，请检查文件是否存在及访问权限。");
            return result;
        }

        const QByteArray data = file.read(64 * 1024);
        result.content = QString::fromUtf8(data);
        result.isError = false;

        core::logging::LoggingService::instance().debug(core::logging::Category::AgentTool, QStringLiteral("read_file 执行完成"), {
            {QStringLiteral("path"), relativePath},
            {QStringLiteral("bytes"), QString::number(data.size())},
            {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
        });

        return result;
    }

} // namespace agent::tool::builtin
