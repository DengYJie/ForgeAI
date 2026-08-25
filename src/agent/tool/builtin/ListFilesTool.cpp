#include "ListFilesTool.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QDir>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace agent::tool::builtin {

    namespace {
        constexpr int MAX_ENTRIES = 500;
    }

    ListFilesTool::ListFilesTool(std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs)
        : m_fs(std::move(fs)) {
        if (!m_fs) {
            m_fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
        }
    }

    domain::agent::ToolDefinition ListFilesTool::definition() const {
        return {
            QStringLiteral("list_files"),
            QStringLiteral("查看指定目录的直接子项列表（非递归）。路径相对于项目根目录。"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("path"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("相对于项目根目录的目录路径，默认 .")}
                    }},
                    {QStringLiteral("include_hidden"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("boolean")},
                        {QStringLiteral("description"), QStringLiteral("是否包含隐藏文件或目录，默认 false")}
                    }}
                }}
            }
        };
    }

    std::unique_ptr<application::ports::IToolOperation> ListFilesTool::execute(
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

    domain::agent::ToolResult ListFilesTool::executeInternal(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
        QElapsedTimer timer;
        timer.start();

        domain::agent::ToolResult result{call.id, {}, true};
        const QJsonObject args = QJsonDocument::fromJson(call.arguments.toUtf8()).object();
        const QString relativePath = args.value(QStringLiteral("path")).toString(QStringLiteral("."));
        const bool includeHidden = args.value(QStringLiteral("include_hidden")).toBool(false);

        QString error;
        const QString path = m_fs->resolveReadablePath(context.workspaceRoot, relativePath, &error);
        if (path.isEmpty()) {
            core::logging::LoggingService::instance().warn(core::logging::Category::AgentTool, QStringLiteral("list_files 路径校验失败"), {
                {QStringLiteral("path"), relativePath},
                {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
            });
            result.content = error.isEmpty() ? QStringLiteral("出于安全原因，无法访问项目外的路径。") : error;
            result.errorCode = QStringLiteral("PathValidationFailed");
            return result;
        }

        const QDir dir(path);
        if (!dir.exists()) {
            core::logging::LoggingService::instance().warn(core::logging::Category::AgentTool, QStringLiteral("list_files 目录不存在"), {
                {QStringLiteral("path"), relativePath},
                {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
            });
            result.content = QStringLiteral("目录不存在: ") + relativePath;
            result.errorCode = QStringLiteral("DirectoryNotFound");
            return result;
        }

        QDir::Filters filters = QDir::NoDotAndDotDot | QDir::AllEntries;
        if (includeHidden) {
            filters |= QDir::Hidden;
        }

        QJsonArray entriesArray;
        bool truncated = false;

        const auto entryList = dir.entryInfoList(filters, QDir::DirsFirst | QDir::Name);
        for (const QFileInfo& entry : entryList) {
            if (context.cancellationToken.isCanceled()) {
                result.content = QStringLiteral("列出文件已被取消");
                result.isError = true;
                result.errorCode = QStringLiteral("Cancelled");
                return result;
            }

            if (!includeHidden && entry.fileName().startsWith(QLatin1Char('.'))) {
                continue;
            }

            const QString relEntry = QDir(context.workspaceRoot).relativeFilePath(entry.absoluteFilePath());
            if (m_fs->isIgnored(relEntry)) continue;

            if (entriesArray.size() >= MAX_ENTRIES) {
                truncated = true;
                break;
            }

            QJsonObject item;
            item[QStringLiteral("name")] = entry.fileName();
            item[QStringLiteral("type")] = entry.isDir() ? QStringLiteral("directory") : QStringLiteral("file");
            entriesArray.append(item);
        }

        QJsonObject rootObj;
        rootObj[QStringLiteral("path")] = relativePath;
        rootObj[QStringLiteral("entries")] = entriesArray;
        rootObj[QStringLiteral("truncated")] = truncated;

        QJsonObject meta;
        meta[QStringLiteral("path")] = relativePath;
        meta[QStringLiteral("count")] = entriesArray.size();
        meta[QStringLiteral("truncated")] = truncated;

        result.content = QString::fromUtf8(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
        result.isError = false;
        result.metadata = meta;

        core::logging::LoggingService::instance().debug(core::logging::Category::AgentTool, QStringLiteral("list_files 执行完成"), {
            {QStringLiteral("path"), relativePath},
            {QStringLiteral("count"), QString::number(entriesArray.size())},
            {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
        });

        return result;
    }

} // namespace agent::tool::builtin
