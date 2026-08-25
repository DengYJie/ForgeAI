#include "SearchTextTool.h"

#include <QDir>
#include <QFile>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace agent::tool::builtin {

    SearchTextTool::SearchTextTool(std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs)
        : m_fs(std::move(fs)) {
        if (!m_fs) {
            m_fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
        }
    }

    domain::agent::ToolDefinition SearchTextTool::definition() const {
        return {
            QStringLiteral("search_text"),
            QStringLiteral("在工作区文件中按不区分大小写方式搜索文本，最多返回 100 个命中。"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("query"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("要搜索的文本")}
                    }},
                    {QStringLiteral("path"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("相对项目根目录的起始目录，默认为 .")}
                    }}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("query")}}
            }
        };
    }

    domain::agent::ToolResult SearchTextTool::execute(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
        domain::agent::ToolResult result{call.id, {}, true};
        const QJsonObject args = QJsonDocument::fromJson(call.arguments.toUtf8()).object();
        const QString query = args.value(QStringLiteral("query")).toString();

        if (query.isEmpty()) {
            result.content = QStringLiteral("缺少 query 参数");
            return result;
        }

        const QString relativeStart = args.value(QStringLiteral("path")).toString(QStringLiteral("."));
        QString error;
        const QString startPath = m_fs->resolveReadablePath(context.workspaceRoot, relativeStart, &error);
        if (startPath.isEmpty()) {
            result.content = error.isEmpty() ? QStringLiteral("路径不合法或超出工作区") : error;
            return result;
        }

        QJsonArray hits;
        QDirIterator it(startPath, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext() && hits.size() < 100) {
            const QString filePath = it.next();
            const QString relPath = QDir(context.workspaceRoot).relativeFilePath(filePath);
            if (m_fs->isIgnored(relPath)) continue;

            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text) || file.size() > 1024 * 1024) {
                continue;
            }

            const QString content = QString::fromUtf8(file.readAll());
            const int offset = content.indexOf(query, 0, Qt::CaseInsensitive);
            if (offset >= 0) {
                const int line = content.left(offset).count(QLatin1Char('\n')) + 1;
                hits.append(QJsonObject{
                    {QStringLiteral("path"), relPath},
                    {QStringLiteral("line"), line}
                });
            }
        }

        result.content = QString::fromUtf8(QJsonDocument(hits).toJson(QJsonDocument::Compact));
        result.isError = false;
        return result;
    }

} // namespace agent::tool::builtin
