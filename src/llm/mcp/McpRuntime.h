#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <memory>
#include "domain/mcp/McpServerTrust.h"
#include "McpServerRegistry.h"
#include "McpTransportFactory.h"
#include "McpSession.h"
#include "McpToolProvider.h"

namespace llm::mcp {

    class McpResourceProvider;
    class McpPromptProvider;

    /**
     * @brief MCP 服务运行时管理器（专职负责多个 McpSession 的生命周期调度、项目隔离与工具提供者联动）
     */
    class McpRuntime : public QObject {
        Q_OBJECT
    public:
        explicit McpRuntime(
            McpServerRegistry* registry = nullptr,
            std::unique_ptr<McpTransportFactory> transportFactory = nullptr,
            QObject* parent = nullptr
        );
        ~McpRuntime() override;

        /**
         * @brief 绑定配置注册表
         */
        void setRegistry(McpServerRegistry* registry);

        /**
         * @brief 获取关联的配置注册表
         */
        McpServerRegistry* registry() const;

        /**
         * @brief 设置安全信任策略
         */
        void setTrustPolicy(const domain::mcp::McpServerTrustPolicy& policy);

        /**
         * @brief 获取安全信任策略引用
         */
        domain::mcp::McpServerTrustPolicy& trustPolicy();
        const domain::mcp::McpServerTrustPolicy& trustPolicy() const;

        /**
         * @brief 启动指定 ID 的 MCP 服务
         */
        bool startServer(const QString& id);

        /**
         * @brief 停止指定 ID 的 MCP 服务
         */
        void stopServer(const QString& id);

        /**
         * @brief 重启指定 ID 的 MCP 服务
         */
        bool restartServer(const QString& id);

        /**
         * @brief 启动所有在注册表中已启用 (enabled) 的 MCP 服务
         */
        void startEnabledServers();

        /**
         * @brief 停止所有运行中的 MCP 服务会话
         */
        void stopAll();

        /**
         * @brief 停止并清理属于指定项目工作区的 MCP 会话
         */
        void stopServersForProject(const QString& workspaceRoot);

        /**
         * @brief 获取指定 ID 的会话指针
         */
        McpSession* session(const QString& id) const;

        /**
         * @brief 获取所有会话列表
         */
        QList<McpSession*> allSessions() const;

        /**
         * @brief 获取 MCP 工具提供者实例
         */
        std::shared_ptr<McpToolProvider> toolProvider() const;

        /**
         * @brief 获取 MCP 资源提供者实例
         */
        std::shared_ptr<McpResourceProvider> resourceProvider() const;

        /**
         * @brief 获取 MCP Prompt 提供者实例
         */
        std::shared_ptr<McpPromptProvider> promptProvider() const;

    Q_SIGNALS:
        void serverStarted(const QString& id);
        void serverStopped(const QString& id);
        void serverError(const QString& id, const QString& error);

    private Q_SLOTS:
        void onServerRegistered(const domain::mcp::McpServerConfig& config);
        void onServerUnregistered(const QString& id);

    private:
        void initSession(const domain::mcp::McpServerConfig& config);

        McpServerRegistry* m_registry = nullptr;
        std::unique_ptr<McpTransportFactory> m_transportFactory;
        domain::mcp::McpServerTrustPolicy m_trustPolicy;
        std::shared_ptr<McpToolProvider> m_toolProvider;
        std::shared_ptr<McpResourceProvider> m_resourceProvider;
        std::shared_ptr<McpPromptProvider> m_promptProvider;
        QMap<QString, std::shared_ptr<McpSession>> m_sessions;
    };

} // namespace llm::mcp
