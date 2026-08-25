#include "WriteFileTool.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace agent::tool::builtin {

    WriteFileTool::WriteFileTool(std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs)
        : m_fs(std::move(fs)) {
        if (!m_fs) {
            m_fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
        }
    }

    domain::agent::ToolDefinition WriteFileTool::definition() const {
        return {
            QStringLiteral("write_file"),
            QStringLiteral("创建或覆盖工作区内的 UTF-8 文本文件。仅在用户明确要求修改项目时使用。"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("path"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("相对项目根目录的文件路径")}
                    }},
                    {QStringLiteral("content"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("要写入的文件文本内容")}
                    }}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("path"), QStringLiteral("content")}}
            }
        };
    }

    std::unique_ptr<application::ports::IToolOperation> WriteFileTool::execute(
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

    domain::agent::ToolResult WriteFileTool::executeInternal(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
        QElapsedTimer timer;
        timer.start();

        domain::agent::ToolResult result{call.id, {}, true};
        const QJsonObject args = QJsonDocument::fromJson(call.arguments.toUtf8()).object();
        const QString relativePath = args.value(QStringLiteral("path")).toString();

        if (context.cancellationToken.isCanceled()) {
            result.content = QStringLiteral("写入文件已被取消");
            return result;
        }

        if (relativePath.trimmed().isEmpty()) {
            result.content = QStringLiteral("缺少 path 参数");
            return result;
        }

        QString error;
        const QString path = m_fs->resolveWritablePath(context.workspaceRoot, relativePath, &error);
        if (path.isEmpty()) {
            core::logging::LoggingService::instance().warn(core::logging::Category::AgentTool, QStringLiteral("write_file 路径校验失败"), {
                {QStringLiteral("path"), relativePath},
                {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
            });
            result.content = error.isEmpty() ? QStringLiteral("出于安全原因，无法访问项目外的路径。") : error;
            return result;
        }

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            core::logging::LoggingService::instance().warn(core::logging::Category::AgentTool, QStringLiteral("write_file 无法打开文件写入"), {
                {QStringLiteral("path"), relativePath},
                {QStringLiteral("osError"), file.errorString()},
                {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
            });
            result.content = QStringLiteral("无法写入该文件，请检查目录权限。");
            return result;
        }

        const QByteArray bytesToWrite = args.value(QStringLiteral("content")).toString().toUtf8();
        file.write(bytesToWrite);
        file.close();

        result.content = QStringLiteral("已写入 ") + QDir(context.workspaceRoot).relativeFilePath(path);
        result.isError = false;

        core::logging::LoggingService::instance().debug(core::logging::Category::AgentTool, QStringLiteral("write_file 执行完成"), {
            {QStringLiteral("path"), relativePath},
            {QStringLiteral("bytes"), QString::number(bytesToWrite.size())},
            {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
        });

        return result;
    }

} // namespace agent::tool::builtin
