#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QEventLoop>
#include <QTimer>
#include <atomic>
#include <optional>
#include "IMcpTransport.h"
#include "domain/agent/ToolDefinition.h"
#include "domain/agent/ToolExecution.h"
#include "domain/mcp/McpResource.h"
#include "domain/mcp/McpPrompt.h"
#include "application/ports/ITool.h"

namespace llm::mcp {

    /**
     * @brief JSON-RPC 2.0 协议层客户端（管理请求 ID、握手版本校验、Tool/Resource/Prompt 协议交互）
     */
    class McpClient : public QObject {
        Q_OBJECT
    public:
        explicit McpClient(IMcpTransport* transport, QObject* parent = nullptr);
        ~McpClient() override = default;

        /**
         * @brief 发起 MCP 初始化握手（包含 protocolVersion 兼容性校验）
         */
        bool initialize(int timeoutMs = 5000);

        /**
         * @brief 获取 MCP 服务暴露的工具声明列表
         */
        QList<domain::agent::ToolDefinition> listTools(int timeoutMs = 5000);

        /**
         * @brief 获取 MCP 服务暴露的资源声明列表
         */
        QList<domain::mcp::McpResource> listResources(int timeoutMs = 5000);

        /**
         * @brief 读取指定 URI 的资源内容
         */
        std::optional<domain::mcp::McpResourceContent> readResource(const QString& uri, int timeoutMs = 5000);

        /**
         * @brief 获取 MCP 服务暴露的 Prompt 列表
         */
        QList<domain::mcp::McpPrompt> listPrompts(int timeoutMs = 5000);

        /**
         * @brief 渲染并获取指定 Prompt 消息
         */
        QList<domain::mcp::McpPromptMessage> getPrompt(
            const QString& name,
            const QJsonObject& arguments = {},
            int timeoutMs = 5000
        );

        /**
         * @brief 调用指定 MCP 工具并获取执行结果（在主线程事件循环中非阻塞限时与取消）
         */
        domain::agent::ToolResult callTool(
            const QString& toolCallId,
            const QString& name,
            const QString& argumentsJson,
            int timeoutMs = 30000,
            application::ports::CancellationToken cancellationToken = {}
        );

        using McpResponseCallback = std::function<void(const QJsonObject& response, bool isError, const QString& errorMessage)>;

        /**
         * @brief 发起非阻塞异步 JSON-RPC 请求
         * @return 请求 ID（可用于取消）
         */
        int sendRequestAsync(
            const QString& method,
            const QJsonObject& params,
            int timeoutMs,
            McpResponseCallback callback
        );

        /**
         * @brief 取消指定的待处理异步请求
         */
        void cancelRequest(int requestId);

        /**
         * @brief 异步调用指定 MCP 工具并返回非阻塞 IToolOperation
         */
        std::unique_ptr<application::ports::IToolOperation> callToolAsync(
            const QString& toolCallId,
            const QString& name,
            const QString& argumentsJson,
            int timeoutMs = 30000,
            application::ports::CancellationToken cancellationToken = {}
        );

        /**
         * @brief 发送通用 JSON-RPC 请求并同步等待回复（初始化/元数据加载使用）
         */
        QJsonObject sendRequestSync(
            const QString& method,
            const QJsonObject& params,
            int timeoutMs = 5000,
            application::ports::CancellationToken cancellationToken = {}
        );

        /**
         * @brief 发送通知（无需对端回复）
         */
        bool sendNotification(const QString& method, const QJsonObject& params);

        /**
         * @brief 获取最后一次握手或请求错误详情
         */
        QString lastError() const { return m_lastError; }

    private Q_SLOTS:
        void onMessageReceived(const QJsonObject& message);

    private:
        struct PendingAsyncRequest {
            int id = 0;
            QString method;
            McpResponseCallback callback;
            QTimer* timeoutTimer = nullptr;
        };

        bool isProtocolVersionSupported(const QString& version) const;

        IMcpTransport* m_transport = nullptr;
        std::atomic<int> m_nextRequestId{1};
        QHash<int, QJsonObject> m_pendingResponses;
        QHash<int, QEventLoop*> m_activeLoops;
        QHash<int, PendingAsyncRequest> m_pendingAsyncRequests;
        QString m_lastError;
    };

} // namespace llm::mcp
