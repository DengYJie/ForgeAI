#pragma once

#include <QObject>
#include <QList>
#include <memory>
#include <mutex>
#include <optional>
#include "domain/mcp/McpResource.h"
#include "McpSession.h"

namespace llm::mcp {

    class McpRuntime;

    /**
     * @brief MCP 资源提供者（汇总管理已激活 MCP 会话暴露的 Resource 声明与内容读取）
     */
    class McpResourceProvider : public QObject {
        Q_OBJECT
    public:
        explicit McpResourceProvider(McpRuntime* runtime = nullptr, QObject* parent = nullptr);
        ~McpResourceProvider() override = default;

        /**
         * @brief 获取当前所有可用 MCP 资源列表
         */
        QList<domain::mcp::McpResource> resources() const;

        /**
         * @brief 读取指定 URI 的资源内容
         */
        std::optional<domain::mcp::McpResourceContent> readResource(const QString& uri, int timeoutMs = 5000);

    private:
        McpRuntime* m_runtime = nullptr;
        mutable std::mutex m_mutex;
    };

} // namespace llm::mcp
