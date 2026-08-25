#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <memory>
#include "McpServerConfig.h"
#include "McpConfigLoader.h"
#include "McpServerRegistry.h"
#include "McpRuntime.h"
#include "McpSession.h"
#include "McpToolProvider.h"

namespace llm::mcp {

    /**
     * @brief MCP 外观门面类（协调 McpConfigLoader、McpServerRegistry 与 McpRuntime）
     */
    class McpManager : public QObject {
        Q_OBJECT
    public:
        explicit McpManager(QObject* parent = nullptr);
        ~McpManager() override = default;

        /**
         * @brief 获取配置注册表
         */
        McpServerRegistry* registry() const;

        /**
         * @brief 获取运行时管理器
         */
        McpRuntime* runtime() const;

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
         * @brief 解析 .mcp.json 或 mcp.json 配置文件
         */
        static QList<domain::mcp::McpServerConfig> parseConfigFile(const QString& filePath);

        /**
         * @brief 解析 MCP 配置 JSON 字符串
         */
        static QList<domain::mcp::McpServerConfig> parseConfigContent(const QString& jsonContent);

        /**
         * @brief 注册 MCP 服务配置
         */
        void registerServer(const domain::mcp::McpServerConfig& config);

        /**
         * @brief 移除指定 MCP 服务
         */
        void unregisterServer(const QString& name);

        /**
         * @brief 启动指定名称的 MCP 服务
         */
        bool startServer(const QString& name);

        /**
         * @brief 停止指定名称的 MCP 服务
         */
        void stopServer(const QString& name);

        /**
         * @brief 启动所有已注册且启用的 MCP 服务
         */
        void startAll();

        /**
         * @brief 停止所有运行中的 MCP 服务
         */
        void stopAll();

        /**
         * @brief 停止并卸载指定工作区项目下的 MCP 服务
         */
        void stopServersForProject(const QString& workspaceRoot);

        /**
         * @brief 获取 MCP 工具提供者实例
         */
        std::shared_ptr<McpToolProvider> toolProvider() const;

        /**
         * @brief 获取指定服务的会话指针
         */
        McpSession* getSession(const QString& name) const;

        /**
         * @brief 获取所有会话列表
         */
        QList<McpSession*> allSessions() const;

    Q_SIGNALS:
        void serverStarted(const QString& name);
        void serverStopped(const QString& name);
        void serverError(const QString& name, const QString& error);

    private:
        std::unique_ptr<McpServerRegistry> m_registry;
        std::unique_ptr<McpRuntime> m_runtime;
    };

} // namespace llm::mcp
