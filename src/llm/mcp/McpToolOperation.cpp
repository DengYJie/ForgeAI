#include "McpToolOperation.h"
#include "McpClient.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QStringList>

namespace llm::mcp {

    McpToolOperation::McpToolOperation(
        McpClient* client,
        const QString& toolCallId,
        const QString& toolName,
        const QJsonObject& params,
        int timeoutMs,
        application::ports::CancellationToken cancellationToken,
        QObject* parent
    ) : application::ports::IToolOperation(parent),
        m_client(client),
        m_toolCallId(toolCallId),
        m_toolName(toolName),
        m_params(params),
        m_timeoutMs(timeoutMs),
        m_cancellationToken(cancellationToken) {
    }

    McpToolOperation::~McpToolOperation() {
        if (m_requestId >= 0 && m_client && m_state == application::ports::ToolOperationState::Running) {
            m_client->cancelRequest(m_requestId);
        }
    }

    QString McpToolOperation::operationId() const {
        return m_toolCallId;
    }

    application::ports::ToolOperationState McpToolOperation::state() const {
        return m_state;
    }

    void McpToolOperation::start() {
        if (m_state != application::ports::ToolOperationState::Created) return;
        m_state = application::ports::ToolOperationState::Running;

        if (m_cancellationToken.isCanceled()) {
            m_state = application::ports::ToolOperationState::Cancelled;
            if (!m_finishedEmitted) {
                m_finishedEmitted = true;
                emit finished(domain::agent::ToolResult{m_toolCallId, QStringLiteral("操作已取消"), true});
            }
            return;
        }

        if (!m_client) {
            m_state = application::ports::ToolOperationState::Failed;
            if (!m_finishedEmitted) {
                m_finishedEmitted = true;
                emit finished(domain::agent::ToolResult{m_toolCallId, QStringLiteral("MCP 客户端未就绪"), true});
            }
            return;
        }

        m_requestId = m_client->sendRequestAsync(
            QStringLiteral("tools/call"),
            m_params,
            m_timeoutMs,
            [this](const QJsonObject& response, bool isError, const QString& errorMessage) {
                handleResponse(response, isError, errorMessage);
            }
        );
    }

    void McpToolOperation::cancel() {
        if (m_state == application::ports::ToolOperationState::Running || m_state == application::ports::ToolOperationState::Created) {
            m_state = application::ports::ToolOperationState::Cancelled;
            if (m_requestId >= 0 && m_client) {
                m_client->cancelRequest(m_requestId);
            }
            if (!m_finishedEmitted) {
                m_finishedEmitted = true;
                emit finished(domain::agent::ToolResult{m_toolCallId, QStringLiteral("操作已取消"), true});
            }
        }
    }

    void McpToolOperation::handleResponse(const QJsonObject& response, bool isError, const QString& errorMessage) {
        if (m_finishedEmitted) return;

        if (isError) {
            m_state = (errorMessage == QStringLiteral("服务响应超时，请稍后重试。"))
                ? application::ports::ToolOperationState::TimedOut
                : application::ports::ToolOperationState::Failed;
            m_finishedEmitted = true;
            emit finished(domain::agent::ToolResult{m_toolCallId, errorMessage, true});
            return;
        }

        if (response.contains(QStringLiteral("error"))) {
            m_state = application::ports::ToolOperationState::Failed;
            const auto errObj = response.value(QStringLiteral("error")).toObject();
            const QString errMsg = errObj.value(QStringLiteral("message")).toString();

            core::logging::LoggingService::instance().warn(core::logging::Category::McpProtocol, QStringLiteral("MCP 工具调用返回错误"), {
                {QStringLiteral("toolName"), m_toolName},
                {QStringLiteral("callId"), m_toolCallId},
                {QStringLiteral("error"), errMsg}
            });

            m_finishedEmitted = true;
            emit finished(domain::agent::ToolResult{m_toolCallId, errMsg, true});
            return;
        }

        if (!response.contains(QStringLiteral("result"))) {
            m_state = application::ports::ToolOperationState::Failed;
            m_finishedEmitted = true;
            emit finished(domain::agent::ToolResult{m_toolCallId, QStringLiteral("MCP 服务未返回有效结果"), true});
            return;
        }

        const auto resultObj = response.value(QStringLiteral("result")).toObject();
        const bool resultIsError = resultObj.value(QStringLiteral("isError")).toBool(false);
        const auto contentArr = resultObj.value(QStringLiteral("content")).toArray();

        QStringList textOutputs;
        for (const auto& itemVal : contentArr) {
            if (itemVal.isObject()) {
                const auto itemObj = itemVal.toObject();
                const QString type = itemObj.value(QStringLiteral("type")).toString();
                if (type == QStringLiteral("text")) {
                    textOutputs.append(itemObj.value(QStringLiteral("text")).toString());
                } else if (type == QStringLiteral("image")) {
                    textOutputs.append(QStringLiteral("[Image content: %1]").arg(itemObj.value(QStringLiteral("mimeType")).toString()));
                } else if (type == QStringLiteral("resource")) {
                    textOutputs.append(QStringLiteral("[Embedded resource: %1]").arg(itemObj.value(QStringLiteral("resource")).toObject().value(QStringLiteral("uri")).toString()));
                }
            } else if (itemVal.isString()) {
                textOutputs.append(itemVal.toString());
            }
        }

        QString finalContent = textOutputs.join(QStringLiteral("\n"));
        if (finalContent.isEmpty()) {
            finalContent = QString::fromUtf8(QJsonDocument(resultObj).toJson(QJsonDocument::Compact));
        }

        m_state = resultIsError ? application::ports::ToolOperationState::Failed : application::ports::ToolOperationState::Completed;
        m_finishedEmitted = true;

        core::logging::LoggingService::instance().debug(core::logging::Category::McpProtocol, QStringLiteral("MCP 异步工具调用完成"), {
            {QStringLiteral("toolName"), m_toolName},
            {QStringLiteral("callId"), m_toolCallId},
            {QStringLiteral("isError"), resultIsError ? QStringLiteral("true") : QStringLiteral("false")},
            {QStringLiteral("contentLength"), QString::number(finalContent.length())}
        });

        emit finished(domain::agent::ToolResult{m_toolCallId, finalContent, resultIsError});
    }

} // namespace llm::mcp
