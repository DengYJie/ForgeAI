#include "ApplyPatchTool.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace agent::tool::builtin {

    ApplyPatchTool::ApplyPatchTool(std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs)
        : m_fs(std::move(fs)) {
        if (!m_fs) {
            m_fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
        }
    }

    domain::agent::ToolDefinition ApplyPatchTool::definition() const {
        return {
            QStringLiteral("apply_patch"),
            QStringLiteral("修改现有文件代码。通过提供精确且唯一的 old_text 和对应的 new_text 结构化补丁进行原子修改。所有补丁必须先全部唯一定位通过后才会一起应用落盘。"),
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("path"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"), QStringLiteral("相对项目根目录的文件路径")}
                    }},
                    {QStringLiteral("patches"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("array")},
                        {QStringLiteral("description"), QStringLiteral("要应用的补丁列表，按顺序在内存中替换")},
                        {QStringLiteral("items"), QJsonObject{
                            {QStringLiteral("type"), QStringLiteral("object")},
                            {QStringLiteral("properties"), QJsonObject{
                                {QStringLiteral("old_text"), QJsonObject{
                                    {QStringLiteral("type"), QStringLiteral("string")},
                                    {QStringLiteral("description"), QStringLiteral("需要被替换的原文件内容片段（必须在文件中精确出现且仅出现 1 次）")}
                                }},
                                {QStringLiteral("new_text"), QJsonObject{
                                    {QStringLiteral("type"), QStringLiteral("string")},
                                    {QStringLiteral("description"), QStringLiteral("替换后的新内容片段")}
                                }}
                            }},
                            {QStringLiteral("required"), QJsonArray{QStringLiteral("old_text"), QStringLiteral("new_text")}}
                        }}
                    }}
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("path"), QStringLiteral("patches")}}
            }
        };
    }

    std::unique_ptr<application::ports::IToolOperation> ApplyPatchTool::execute(
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

    domain::agent::ToolResult ApplyPatchTool::executeInternal(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
        QElapsedTimer timer;
        timer.start();

        domain::agent::ToolResult result{call.id, {}, true};
        const QJsonObject args = QJsonDocument::fromJson(call.arguments.toUtf8()).object();
        const QString relativePath = args.value(QStringLiteral("path")).toString();
        const QJsonArray patchesArray = args.value(QStringLiteral("patches")).toArray();

        if (context.cancellationToken.isCanceled()) {
            result.content = QStringLiteral("补丁应用操作已被取消");
            result.errorCode = QStringLiteral("Cancelled");
            return result;
        }

        if (relativePath.trimmed().isEmpty()) {
            result.content = QStringLiteral("缺少 path 参数");
            result.errorCode = QStringLiteral("MissingParameter");
            return result;
        }

        if (patchesArray.isEmpty()) {
            result.content = QStringLiteral("缺少 patches 参数或补丁列表为空");
            result.errorCode = QStringLiteral("EmptyPatches");
            return result;
        }

        QString error;
        const QString path = m_fs->resolveWritablePath(context.workspaceRoot, relativePath, &error);
        if (path.isEmpty()) {
            core::logging::LoggingService::instance().warn(core::logging::Category::AgentTool, QStringLiteral("apply_patch 路径校验失败"), {
                {QStringLiteral("path"), relativePath},
                {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
            });
            result.content = error.isEmpty() ? QStringLiteral("出于安全原因，无法访问项目外的路径。") : error;
            result.errorCode = QStringLiteral("PathValidationFailed");
            return result;
        }

        QFile file(path);
        if (!file.exists()) {
            result.content = QStringLiteral("目标文件不存在，无法应用补丁: ") + relativePath;
            result.errorCode = QStringLiteral("FileNotFound");
            return result;
        }

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            result.content = QStringLiteral("无法读取目标文件: ") + file.errorString();
            result.errorCode = QStringLiteral("FileReadError");
            return result;
        }

        QString fileContent = QString::fromUtf8(file.readAll());
        file.close();

        // 1. 结构化内存预校验：模拟逐项替换并确保每个 old_text 均恰好匹配 1 次
        QString tempContent = fileContent;
        struct PatchItem {
            QString oldText;
            QString newText;
        };
        QList<PatchItem> parsedPatches;

        for (int i = 0; i < patchesArray.size(); ++i) {
            const QJsonObject patchObj = patchesArray[i].toObject();
            const QString oldText = patchObj.value(QStringLiteral("old_text")).toString();
            const QString newText = patchObj.value(QStringLiteral("new_text")).toString();

            if (oldText.isEmpty()) {
                result.content = QStringLiteral("补丁 #%1 的 old_text 为空").arg(i + 1);
                result.errorCode = QStringLiteral("InvalidPatch");
                return result;
            }

            int count = 0;
            int pos = 0;
            while ((pos = tempContent.indexOf(oldText, pos)) != -1) {
                ++count;
                pos += oldText.length();
            }

            if (count == 0) {
                core::logging::LoggingService::instance().warn(core::logging::Category::AgentTool, QStringLiteral("apply_patch 匹配失败 (0次)"), {
                    {QStringLiteral("path"), relativePath},
                    {QStringLiteral("patchIndex"), QString::number(i + 1)}
                });
                result.content = QStringLiteral("补丁 #%1 的 old_text 在目标文件中未找到匹配项。请先使用 read_file 获取最新文件内容后再重试。").arg(i + 1);
                result.errorCode = QStringLiteral("PatchContextNotFound");
                return result;
            }

            if (count > 1) {
                core::logging::LoggingService::instance().warn(core::logging::Category::AgentTool, QStringLiteral("apply_patch 存在歧义匹配 (多次)"), {
                    {QStringLiteral("path"), relativePath},
                    {QStringLiteral("patchIndex"), QString::number(i + 1)},
                    {QStringLiteral("matches"), QString::number(count)}
                });
                result.content = QStringLiteral("补丁 #%1 的 old_text 在目标文件中出现多处 (%2 次)，存在定位歧义。请增加上下文行以唯一定位。").arg(i + 1).arg(count);
                result.errorCode = QStringLiteral("PatchContextAmbiguous");
                return result;
            }

            // 预演替换
            const int matchIdx = tempContent.indexOf(oldText);
            tempContent.replace(matchIdx, oldText.length(), newText);
            parsedPatches.append({oldText, newText});
        }

        // 2. 预校验全部通过，使用 QSaveFile 进行原子落盘
        QSaveFile saveFile(path);
        if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            result.content = QStringLiteral("无法打开文件进行原子写入: ") + saveFile.errorString();
            result.errorCode = QStringLiteral("FileOpenError");
            return result;
        }

        const QByteArray finalBytes = tempContent.toUtf8();
        saveFile.write(finalBytes);
        if (!saveFile.commit()) {
            result.content = QStringLiteral("补丁原子提交失败: ") + saveFile.errorString();
            result.errorCode = QStringLiteral("FileCommitError");
            return result;
        }

        QJsonObject rootObj;
        rootObj[QStringLiteral("path")] = relativePath;
        rootObj[QStringLiteral("patch_count")] = parsedPatches.size();
        rootObj[QStringLiteral("changed")] = true;

        QJsonObject meta;
        meta[QStringLiteral("path")] = relativePath;
        meta[QStringLiteral("patch_count")] = parsedPatches.size();
        meta[QStringLiteral("changed")] = true;

        result.content = QString::fromUtf8(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
        result.isError = false;
        result.metadata = meta;

        core::logging::LoggingService::instance().debug(core::logging::Category::AgentTool, QStringLiteral("apply_patch 执行完成"), {
            {QStringLiteral("path"), relativePath},
            {QStringLiteral("patches"), QString::number(parsedPatches.size())},
            {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
        });

        return result;
    }

} // namespace agent::tool::builtin
