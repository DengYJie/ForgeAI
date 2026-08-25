#include "SearchTextTool.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QTextStream>

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
            QStringLiteral("在工作区文本文件中搜索指定字符串或正则表达式，返回匹配项的行号、列号与代码片段预览。"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("query"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("要搜索的文本内容或正则表达式模式")}
                    }},
                    {QStringLiteral("path"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("相对项目根目录的起始目录，默认为 .")}
                    }},
                    {QStringLiteral("case_sensitive"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("boolean")},
                        {QStringLiteral("description"), QStringLiteral("是否区分大小写，默认 false")}
                    }},
                    {QStringLiteral("regex"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("boolean")},
                        {QStringLiteral("description"), QStringLiteral("是否使用正则表达式搜索，默认 false")}
                    }},
                    {QStringLiteral("file_pattern"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("文件名通配过滤模式，如 *.cpp、*.h 或 *.json")}
                    }},
                    {QStringLiteral("max_results"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("integer")},
                        {QStringLiteral("minimum"), 1},
                        {QStringLiteral("maximum"), 200},
                        {QStringLiteral("description"), QStringLiteral("最大返回匹配结果数量，默认 50，最大 200")}
                    }}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("query")}}
            }
        };
    }

    std::unique_ptr<application::ports::IToolOperation> SearchTextTool::execute(
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

    domain::agent::ToolResult SearchTextTool::executeInternal(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
        QElapsedTimer timer;
        timer.start();

        domain::agent::ToolResult result{call.id, {}, true};
        const QJsonObject args = QJsonDocument::fromJson(call.arguments.toUtf8()).object();
        const QString query = args.value(QStringLiteral("query")).toString();

        if (query.isEmpty()) {
            result.content = QStringLiteral("缺少 query 参数");
            result.errorCode = QStringLiteral("MissingParameter");
            return result;
        }

        const QString relativeStart = args.value(QStringLiteral("path")).toString(QStringLiteral("."));
        const bool caseSensitive = args.value(QStringLiteral("case_sensitive")).toBool(false);
        const bool isRegex = args.value(QStringLiteral("regex")).toBool(false);
        const QString filePattern = args.value(QStringLiteral("file_pattern")).toString();
        const int maxResults = qBound(1, args.value(QStringLiteral("max_results")).toInt(50), 200);

        QRegularExpression regexPattern;
        if (isRegex) {
            QRegularExpression::PatternOptions patternOptions = QRegularExpression::NoPatternOption;
            if (!caseSensitive) {
                patternOptions |= QRegularExpression::CaseInsensitiveOption;
            }
            regexPattern = QRegularExpression(query, patternOptions);
            if (!regexPattern.isValid()) {
                result.content = QStringLiteral("正则表达式模式非法: ") + regexPattern.errorString();
                result.errorCode = QStringLiteral("InvalidRegex");
                return result;
            }
        }

        QString error;
        const QString startPath = m_fs->resolveReadablePath(context.workspaceRoot, relativeStart, &error);
        if (startPath.isEmpty()) {
            core::logging::LoggingService::instance().warn(core::logging::Category::AgentTool, QStringLiteral("search_text 路径校验失败"), {
                {QStringLiteral("path"), relativeStart},
                {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
            });
            result.content = error.isEmpty() ? QStringLiteral("出于安全原因，无法访问项目外的路径。") : error;
            result.errorCode = QStringLiteral("PathValidationFailed");
            return result;
        }

        QJsonArray matchesArray;
        bool truncated = false;

        QDirIterator it(startPath, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (context.cancellationToken.isCanceled()) {
                result.content = QStringLiteral("搜索操作已被取消");
                result.errorCode = QStringLiteral("Cancelled");
                result.isError = true;
                return result;
            }

            const QString filePath = it.next();
            const QString relPath = QDir(context.workspaceRoot).relativeFilePath(filePath);
            if (m_fs->isIgnored(relPath)) continue;

            const QFileInfo fileInfo(filePath);
            if (!filePattern.isEmpty() && !QDir::match(filePattern, fileInfo.fileName())) {
                continue;
            }

            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024) {
                continue; // 超过 4MB 的文件跳过扫描
            }

            const QByteArray probe = file.peek(512);
            if (probe.contains('\0')) {
                continue; // 二进制文件自动跳过
            }

            QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            stream.setCodec("UTF-8");
#else
            stream.setEncoding(QStringConverter::Utf8);
#endif

            int lineNumber = 0;
            while (!stream.atEnd()) {
                const QString line = stream.readLine();
                ++lineNumber;

                int col = -1;
                if (isRegex) {
                    const auto match = regexPattern.match(line);
                    if (match.hasMatch()) {
                        col = match.capturedStart() + 1;
                    }
                } else {
                    const Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
                    const int idx = line.indexOf(query, 0, cs);
                    if (idx >= 0) {
                        col = idx + 1;
                    }
                }

                if (col > 0) {
                    QJsonObject item;
                    item[QStringLiteral("path")] = relPath;
                    item[QStringLiteral("line")] = lineNumber;
                    item[QStringLiteral("column")] = col;
                    item[QStringLiteral("preview")] = line.trimmed();
                    matchesArray.append(item);

                    if (matchesArray.size() >= maxResults) {
                        truncated = true;
                        break;
                    }
                }
            }

            if (truncated) break;
        }

        QJsonObject rootObj;
        rootObj[QStringLiteral("matches")] = matchesArray;
        rootObj[QStringLiteral("truncated")] = truncated;

        QJsonObject meta;
        meta[QStringLiteral("count")] = matchesArray.size();
        meta[QStringLiteral("truncated")] = truncated;
        meta[QStringLiteral("query")] = query;

        result.content = QString::fromUtf8(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
        result.isError = false;
        result.metadata = meta;

        core::logging::LoggingService::instance().debug(core::logging::Category::AgentTool, QStringLiteral("search_text 执行完成"), {
            {QStringLiteral("startPath"), relativeStart},
            {QStringLiteral("matchesCount"), QString::number(matchesArray.size())},
            {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
        });

        return result;
    }

} // namespace agent::tool::builtin
