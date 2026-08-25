#include "ReadFileTool.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QFile>
#include <QTextStream>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace agent::tool::builtin {

    namespace {
        constexpr int MAX_LINES = 500;
        constexpr int MAX_BYTES = 128 * 1024; // 128 KB
        constexpr int BINARY_CHECK_BYTES = 512;
    }

    ReadFileTool::ReadFileTool(std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs)
        : m_fs(std::move(fs)) {
        if (!m_fs) {
            m_fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
        }
    }

    domain::agent::ToolDefinition ReadFileTool::definition() const {
        return {
            QStringLiteral("read_file"),
            QStringLiteral("读取工作区内的文本文件内容，支持按起始行与结束行范围精准读取，返回带有行号的代码内容。"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("path"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("相对项目根目录的文件路径")}
                    }},
                    {QStringLiteral("start_line"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("integer")},
                        {QStringLiteral("minimum"), 1},
                        {QStringLiteral("description"), QStringLiteral("起始行号（从 1 开始，包含此行），默认 1")}
                    }},
                    {QStringLiteral("end_line"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("integer")},
                        {QStringLiteral("minimum"), 1},
                        {QStringLiteral("description"), QStringLiteral("结束行号（包含此行），默认读取至文件末尾或最大允许行数")}
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
            result.errorCode = QStringLiteral("Cancelled");
            return result;
        }

        if (relativePath.trimmed().isEmpty()) {
            result.content = QStringLiteral("缺少 path 参数");
            result.errorCode = QStringLiteral("MissingParameter");
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
            result.errorCode = QStringLiteral("PathValidationFailed");
            return result;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            core::logging::LoggingService::instance().warn(core::logging::Category::AgentTool, QStringLiteral("read_file 无法打开文件"), {
                {QStringLiteral("path"), relativePath},
                {QStringLiteral("osError"), file.errorString()},
                {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
            });
            result.content = QStringLiteral("无法读取该文件，请检查文件是否存在及访问权限。");
            result.errorCode = QStringLiteral("FileNotFoundOrPermissionDenied");
            return result;
        }

        // 1. 二进制文件检测
        const QByteArray probe = file.peek(BINARY_CHECK_BYTES);
        if (probe.contains('\0')) {
            core::logging::LoggingService::instance().warn(core::logging::Category::AgentTool, QStringLiteral("read_file 检测到二进制文件"), {
                {QStringLiteral("path"), relativePath}
            });
            result.content = QStringLiteral("不支持读取二进制文件。");
            result.errorCode = QStringLiteral("UnsupportedBinaryFile");
            return result;
        }

        int reqStartLine = args.contains(QStringLiteral("start_line")) ? args.value(QStringLiteral("start_line")).toInt(1) : 1;
        if (reqStartLine < 1) reqStartLine = 1;
        const int reqEndLine = args.contains(QStringLiteral("end_line")) ? args.value(QStringLiteral("end_line")).toInt(-1) : -1;

        QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        stream.setCodec("UTF-8");
#else
        stream.setEncoding(QStringConverter::Utf8);
#endif

        int currentLine = 0;
        int totalLines = 0;
        int linesRead = 0;
        int bytesRead = 0;
        bool truncated = false;
        int actualStart = reqStartLine;
        int actualEnd = reqStartLine;

        QStringList formattedLines;
        while (!stream.atEnd()) {
            if (context.cancellationToken.isCanceled()) {
                result.content = QStringLiteral("读取文件已被取消");
                result.errorCode = QStringLiteral("Cancelled");
                return result;
            }

            const QString line = stream.readLine();
            ++currentLine;
            totalLines = currentLine;

            if (currentLine < reqStartLine) {
                continue;
            }

            if (reqEndLine > 0 && currentLine > reqEndLine) {
                // 已经读取完指定范围，但为了获取 totalLines 继续扫描或提前退出
                // 快速跳过剩余行计数
                while (!stream.atEnd()) {
                    stream.readLine();
                    ++totalLines;
                }
                break;
            }

            if (linesRead >= MAX_LINES || bytesRead >= MAX_BYTES) {
                truncated = true;
                while (!stream.atEnd()) {
                    stream.readLine();
                    ++totalLines;
                }
                break;
            }

            formattedLines.append(QStringLiteral("%1 | %2").arg(currentLine, 6).arg(line));
            actualEnd = currentLine;
            ++linesRead;
            bytesRead += line.toUtf8().size() + 1;
        }

        if (linesRead == 0 && reqStartLine > totalLines) {
            actualStart = totalLines > 0 ? totalLines : 1;
            actualEnd = actualStart;
        }

        const QString formattedContent = formattedLines.join(QLatin1Char('\n'));

        QJsonObject rootObj;
        rootObj[QStringLiteral("path")] = relativePath;
        rootObj[QStringLiteral("start_line")] = actualStart;
        rootObj[QStringLiteral("end_line")] = actualEnd;
        rootObj[QStringLiteral("total_lines")] = totalLines;
        rootObj[QStringLiteral("content")] = formattedContent;
        rootObj[QStringLiteral("truncated")] = truncated;
        if (truncated) {
            rootObj[QStringLiteral("next_start_line")] = actualEnd + 1;
        }

        QJsonObject meta;
        meta[QStringLiteral("path")] = relativePath;
        meta[QStringLiteral("start_line")] = actualStart;
        meta[QStringLiteral("end_line")] = actualEnd;
        meta[QStringLiteral("total_lines")] = totalLines;
        meta[QStringLiteral("bytes")] = bytesRead;
        meta[QStringLiteral("truncated")] = truncated;

        result.content = QString::fromUtf8(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
        result.isError = false;
        result.metadata = meta;

        core::logging::LoggingService::instance().debug(core::logging::Category::AgentTool, QStringLiteral("read_file 执行完成"), {
            {QStringLiteral("path"), relativePath},
            {QStringLiteral("linesRead"), QString::number(linesRead)},
            {QStringLiteral("totalLines"), QString::number(totalLines)},
            {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
        });

        return result;
    }

} // namespace agent::tool::builtin
