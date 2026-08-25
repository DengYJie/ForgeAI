#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <optional>
#include "domain/mcp/McpServerConfig.h"

namespace llm::mcp {

    /**
     * @brief MCP 服务配置注册表（专职管理“有什么”静态配置，严禁包含任何进程调度与通信逻辑）
     */
    class McpServerRegistry : public QObject {
        Q_OBJECT
    public:
        explicit McpServerRegistry(QObject* parent = nullptr);
        ~McpServerRegistry() override = default;

        /**
         * @brief 注册或更新单个 MCP 服务配置
         */
        void registerServer(const domain::mcp::McpServerConfig& config);

        /**
         * @brief 批量注册或覆盖 MCP 服务配置
         */
        void registerServers(const QList<domain::mcp::McpServerConfig>& configs);

        /**
         * @brief 移除指定 ID 的 MCP 服务配置
         */
        void unregisterServer(const QString& id);

        /**
         * @brief 查询指定 ID 的服务配置
         */
        std::optional<domain::mcp::McpServerConfig> server(const QString& id) const;

        /**
         * @brief 检查是否存在指定 ID 的服务配置
         */
        bool hasServer(const QString& id) const;

        /**
         * @brief 获取所有已注册的 MCP 服务配置列表
         */
        QList<domain::mcp::McpServerConfig> servers() const;

        /**
         * @brief 清空所有已注册配置
         */
        void clear();

    Q_SIGNALS:
        void serverRegistered(const domain::mcp::McpServerConfig& config);
        void serverUnregistered(const QString& id);
        void registryChanged();

    private:
        QMap<QString, domain::mcp::McpServerConfig> m_servers;
    };

} // namespace llm::mcp
