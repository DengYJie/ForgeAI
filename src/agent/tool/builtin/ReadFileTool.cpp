#include "ReadFileTool.h"

#include <QFile>
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

    domain::agent::ToolResult ReadFileTool::execute(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
        domain::agent::ToolResult result{call.id, {}, true};
        const QJsonObject args = QJsonDocument::fromJson(call.arguments.toUtf8()).object();
        const QString relativePath = args.value(QStringLiteral("path")).toString();

        if (relativePath.trimmed().isEmpty()) {
            result.content = QStringLiteral("缺少 path 参数");
            return result;
        }

        QString error;
        const QString path = m_fs->resolveReadablePath(context.workspaceRoot, relativePath, &error);
        if (path.isEmpty()) {
            result.content = error.isEmpty() ? QStringLiteral("路径不合法或超出工作区") : error;
            return result;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            result.content = QStringLiteral("无法读取文件: ") + file.errorString();
            return result;
        }

        result.content = QString::fromUtf8(file.read(64 * 1024));
        result.isError = false;
        return result;
    }

} // namespace agent::tool::builtin
