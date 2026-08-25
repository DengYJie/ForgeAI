#pragma once
#include <QString>

namespace core::logging {

    /**
     * @brief 标准化日志模块分类常量子系统
     */
    namespace Category {
        inline const QString App = QStringLiteral("app");
        inline const QString AppLifecycle = QStringLiteral("app.lifecycle");

        inline const QString Network = QStringLiteral("network");
        inline const QString NetworkHttp = QStringLiteral("network.http");
        inline const QString NetworkProxy = QStringLiteral("network.proxy");

        inline const QString Llm = QStringLiteral("llm");
        inline const QString LlmRequest = QStringLiteral("llm.request");
        inline const QString LlmProtocol = QStringLiteral("llm.protocol");
        inline const QString LlmStream = QStringLiteral("llm.stream");
        inline const QString LlmRetry = QStringLiteral("llm.retry");
        inline const QString LlmTimeout = QStringLiteral("llm.timeout");

        inline const QString Provider = QStringLiteral("provider");
        inline const QString ProviderOpenAI = QStringLiteral("provider.openai");
        inline const QString ProviderAnthropic = QStringLiteral("provider.anthropic");
        inline const QString ProviderGemini = QStringLiteral("provider.gemini");
        inline const QString ProviderOllama = QStringLiteral("provider.ollama");

        inline const QString Storage = QStringLiteral("storage");
        inline const QString StorageDatabase = QStringLiteral("storage.database");
        inline const QString StorageSettings = QStringLiteral("storage.settings");

        inline const QString Ui = QStringLiteral("ui");
        inline const QString UiChat = QStringLiteral("ui.chat");
        inline const QString UiNavigation = QStringLiteral("ui.navigation");

        // Agent
        inline const QString AgentRuntime = QStringLiteral("agent.runtime");
        inline const QString AgentTool = QStringLiteral("agent.tool");
        inline const QString AgentSkill = QStringLiteral("agent.skill");

        // Workspace
        inline const QString Workspace = QStringLiteral("workspace");

        // MCP
        inline const QString McpConfig = QStringLiteral("mcp.config");
        inline const QString McpRegistry = QStringLiteral("mcp.registry");
        inline const QString McpSession = QStringLiteral("mcp.session");
        inline const QString McpProtocol = QStringLiteral("mcp.protocol");
        inline const QString McpTransport = QStringLiteral("mcp.transport");
        inline const QString McpSecurity = QStringLiteral("mcp.security");
    }

} // namespace core::logging
