#include "ListFilesTool.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace agent::tool::builtin {

    ListFilesTool::ListFilesTool(std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs)
        : m_fs(std::move(fs)) {
        if (!m_fs) {
            m_fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
        }
    }

    domain::agent::ToolDefinition ListFilesTool::definition() const {
        return {
            QStringLiteral("list_files"),
            QStringLiteral("列出工作区内指定目录的直接内容。路径相对于项目根目录。"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("path"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("相对项目根目录的目录，默认为 .")}
                    }}
                }}
            }
        };
    }

    domain::agent::ToolResult ListFilesTool::execute(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
        domain::agent::ToolResult result{call.id, {}, true};
        const QJsonObject args = QJsonDocument::fromJson(call.arguments.toUtf8()).object();
        const QString relativePath = args.value(QStringLiteral("path")).toString(QStringLiteral("."));

        QString error;
        const QString path = m_fs->resolveReadablePath(context.workspaceRoot, relativePath, &error);
        if (path.isEmpty()) {
            result.content = error.isEmpty() ? QStringLiteral("路径不合法或超出工作区") : error;
            return result;
        }

        const QDir dir(path);
        if (!dir.exists()) {
            result.content = QStringLiteral("目录不存在: ") + relativePath;
            return result;
        }

        QJsonArray files;
        for (const QFileInfo& entry : dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries, QDir::DirsFirst | QDir::Name)) {
            if (context.cancellationToken.isCanceled()) {
                result.content = QStringLiteral("列出文件已被取消");
                result.isError = true;
                return result;
            }
            const QString relEntry = QDir(context.workspaceRoot).relativeFilePath(entry.absoluteFilePath());
            if (m_fs->isIgnored(relEntry)) continue;

            files.append(entry.fileName() + (entry.isDir() ? QStringLiteral("/") : QString()));
        }

        result.content = QString::fromUtf8(QJsonDocument(files).toJson(QJsonDocument::Compact));
        result.isError = false;
        return result;
    }

} // namespace agent::tool::builtin
