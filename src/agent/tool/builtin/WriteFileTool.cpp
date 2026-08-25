#include "WriteFileTool.h"

#include <QDir>
#include <QFile>
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

    domain::agent::ToolResult WriteFileTool::execute(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
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
            result.content = error.isEmpty() ? QStringLiteral("路径不合法或超出工作区") : error;
            return result;
        }

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            result.content = QStringLiteral("无法写入文件: ") + file.errorString();
            return result;
        }

        file.write(args.value(QStringLiteral("content")).toString().toUtf8());
        file.close();

        result.content = QStringLiteral("已写入 ") + QDir(context.workspaceRoot).relativeFilePath(path);
        result.isError = false;
        return result;
    }

} // namespace agent::tool::builtin
