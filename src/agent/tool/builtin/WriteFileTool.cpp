#include "WriteFileTool.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
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
            QStringLiteral("创建新文件，或在明确指定 overwrite=true 时全量覆盖已有文件。若只需修改现有文件的一小部分，应优先使用 apply_patch。"),
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
                    }},
                    {QStringLiteral("overwrite"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("boolean")},
                        {QStringLiteral("description"), QStringLiteral("当文件已存在时是否允许覆盖。默认 false，以防止误破坏已有代码。")}
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
        const bool overwrite = args.value(QStringLiteral("overwrite")).toBool(false);

        if (context.cancellationToken.isCanceled()) {
            result.content = QStringLiteral("写入文件已被取消");
            result.errorCode = QStringLiteral("Cancelled");
            return result;
        }

        if (relativePath.trimmed().isEmpty()) {
            result.content = QStringLiteral("缺少 path 参数");
            result.errorCode = QStringLiteral("MissingParameter");
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
            result.errorCode = QStringLiteral("PathValidationFailed");
            return result;
        }

        const QFileInfo fileInfo(path);
        const bool fileExisted = fileInfo.exists();

        // 1. 防误覆盖检查
        if (fileExisted && !overwrite) {
            result.content = QStringLiteral("文件已存在。若确定要整体覆盖，请指定 overwrite=true，或改用 apply_patch 进行精准局部修改。");
            result.errorCode = QStringLiteral("FileAlreadyExists");
            return result;
        }

        const QString content = args.value(QStringLiteral("content")).toString();
        const QByteArray bytesToWrite = content.toUtf8();

        // 2. QSaveFile 原子写入
        QSaveFile saveFile(path);
        if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            core::logging::LoggingService::instance().warn(core::logging::Category::AgentTool, QStringLiteral("write_file 无法打开文件写入"), {
                {QStringLiteral("path"), relativePath},
                {QStringLiteral("osError"), saveFile.errorString()},
                {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
            });
            result.content = QStringLiteral("无法写入该文件，请检查目录权限: ") + saveFile.errorString();
            result.errorCode = QStringLiteral("FileOpenError");
            return result;
        }

        saveFile.write(bytesToWrite);
        if (!saveFile.commit()) {
            core::logging::LoggingService::instance().warn(core::logging::Category::AgentTool, QStringLiteral("write_file 原子提交失败"), {
                {QStringLiteral("path"), relativePath},
                {QStringLiteral("osError"), saveFile.errorString()}
            });
            result.content = QStringLiteral("文件原子提交失败: ") + saveFile.errorString();
            result.errorCode = QStringLiteral("FileCommitError");
            return result;
        }

        QJsonObject rootObj;
        rootObj[QStringLiteral("path")] = relativePath;
        rootObj[QStringLiteral("bytes_written")] = bytesToWrite.size();
        rootObj[QStringLiteral("created")] = !fileExisted;

        QJsonObject meta;
        meta[QStringLiteral("path")] = relativePath;
        meta[QStringLiteral("bytes_written")] = bytesToWrite.size();
        meta[QStringLiteral("created")] = !fileExisted;

        result.content = QString::fromUtf8(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
        result.isError = false;
        result.metadata = meta;

        core::logging::LoggingService::instance().debug(core::logging::Category::AgentTool, QStringLiteral("write_file 执行完成"), {
            {QStringLiteral("path"), relativePath},
            {QStringLiteral("bytes"), QString::number(bytesToWrite.size())},
            {QStringLiteral("created"), fileExisted ? QStringLiteral("false") : QStringLiteral("true")},
            {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
        });

        return result;
    }

} // namespace agent::tool::builtin
