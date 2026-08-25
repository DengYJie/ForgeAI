#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <memory>
#include "McpServerConfig.h"
#include "McpSession.h"
#include "McpToolProvider.h"

namespace llm::mcp {

    /**
     * @brief 全局/项目级 MCP 服务管理器（负责配置解析、会话调度与工具提供者集成）
     */
    class McpManager : public QObject {
        Q_OBJECT
    public:
        explicit McpManager(QObject* parent = nullptr);
        ~McpManager() override;

        /**
         * @brief 解析 .mcp.json 或 mcp.json 配置文件
         */
        static QList<McpServerConfig> parseConfigFile(const QString& filePath);

        /**
         * @brief 解析 MCP 配置 JSON 字符串
         */
        static QList<McpServerConfig> parseConfigContent(const QString& jsonContent);

        /**
         * @brief 注册 MCP 服务配置
         */
        void registerServer(const McpServerConfig& config);

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
         * @brief 启动所有已注册且未禁用的 MCP 服务
         */
        void startAll();

        /**
         * @brief 停止所有运行中的 MCP 服务
         */
        void stopAll();

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
        std::shared_ptr<McpToolProvider> m_toolProvider;
        QMap<QString, std::shared_ptr<McpSession>> m_sessions;
    };

} // namespace llm::mcp
