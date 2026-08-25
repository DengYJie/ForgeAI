#pragma once

#include "application/ports/ITool.h"
#include "application/ports/IToolOperation.h"
#include <QJsonObject>

namespace llm::mcp {

    class McpClient;

    /**
     * @brief 真正的非阻塞异步 MCP 工具调用操作
     * @details 消除局部 QEventLoop 与定时器轮询，通过 McpClient 异步 JSON-RPC 请求驱动。
     */
    class McpToolOperation final : public application::ports::IToolOperation {
        Q_OBJECT
    public:
        McpToolOperation(
            McpClient* client,
            const QString& toolCallId,
            const QString& toolName,
            const QJsonObject& params,
            int timeoutMs = 30000,
            application::ports::CancellationToken cancellationToken = {},
            QObject* parent = nullptr
        );
        ~McpToolOperation() override;

        QString operationId() const override;
        application::ports::ToolOperationState state() const override;

        void start() override;
        void cancel() override;

    private:
        void handleResponse(const QJsonObject& response, bool isError, const QString& errorMessage);

        McpClient* m_client = nullptr;
        QString m_toolCallId;
        QString m_toolName;
        QJsonObject m_params;
        int m_timeoutMs = 30000;
        application::ports::CancellationToken m_cancellationToken;
        application::ports::ToolOperationState m_state = application::ports::ToolOperationState::Created;
        int m_requestId = -1;
        bool m_finishedEmitted = false;
    };

} // namespace llm::mcp
