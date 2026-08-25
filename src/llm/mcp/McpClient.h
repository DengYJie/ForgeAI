#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QEventLoop>
#include <QTimer>
#include <atomic>
#include "IMcpTransport.h"
#include "domain/agent/ToolDefinition.h"
#include "domain/agent/ToolExecution.h"
#include "application/ports/ITool.h"

namespace llm::mcp {

    /**
     * @brief JSON-RPC 2.0 协议层客户端
     */
    class McpClient : public QObject {
        Q_OBJECT
    public:
        explicit McpClient(IMcpTransport* transport, QObject* parent = nullptr);
        ~McpClient() override = default;

        /**
         * @brief 发起 MCP 初始化握手
         */
        bool initialize(int timeoutMs = 5000);

        /**
         * @brief 获取 MCP 服务暴露的工具声明列表
         */
        QList<domain::agent::ToolDefinition> listTools(int timeoutMs = 5000);

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

        /**
         * @brief 发送通用 JSON-RPC 请求并同步等待回复
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

    private Q_SLOTS:
        void onMessageReceived(const QJsonObject& message);

    private:
        IMcpTransport* m_transport = nullptr;
        std::atomic<int> m_nextRequestId{1};
        QHash<int, QJsonObject> m_pendingResponses;
        QHash<int, QEventLoop*> m_activeLoops;
    };

} // namespace llm::mcp
