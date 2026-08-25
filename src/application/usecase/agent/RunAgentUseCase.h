#pragma once

#include <QObject>
#include <QString>
#include <QUuid>
#include "application/ports/IAgentRuntime.h"
#include "domain/service/IModelService.h"
#include "domain/service/IProjectContextService.h"
#include "agent/runtime/AgentContextBuilder.h"
#include "llm/mcp/McpManager.h"

namespace application::usecase::agent {

    /**
     * @brief 启动与驱动 Agent 执行的业务用例
     */
    class RunAgentUseCase : public QObject {
        Q_OBJECT
    public:
        explicit RunAgentUseCase(
            ports::IAgentRuntime* runtime,
            domain::service::IModelService* modelService,
            domain::service::IProjectContextService* projectContextService = nullptr,
            llm::mcp::McpManager* mcpManager = nullptr,
            QObject* parent = nullptr
        );
        ~RunAgentUseCase() override = default;

        void execute(
            const QString& sessionId,
            const QString& prompt,
            const QUuid& projectId,
            const QString& workspaceRoot,
            const QString& providerId = {},
            const QString& modelId = {},
            bool useWebSearch = false,
            bool useDeepThinking = false,
            const QString& reasoningEffort = {},
            const QString& customSystemPrompt = {}
        );

        void cancelCurrent();
        void grantPermission(const QString& sessionId, const QString& toolCallId, bool granted);
        bool isRunning() const;
        ports::IAgentRuntime* runtime() const;

    Q_SIGNALS:
        void userMessageCreated(const QString& sessionId, const domain::conversation::Message& message);
        void stateChanged(const domain::agent::AgentRunState& state);
        void tokenReceived(const QString& sessionId, const QString& token);
        void thoughtReceived(const QString& sessionId, const QString& thought);
        void toolCallStarted(const QString& sessionId, const domain::agent::ToolCall& call);
        void toolCallFinished(const QString& sessionId, const domain::agent::ToolCall& call);
        void toolResultReady(const QString& sessionId, const domain::agent::ToolResult& result);
        void permissionRequested(const QString& sessionId, const domain::agent::ToolCall& call, const domain::agent::ToolPermission& permission);
        void replyGenerated(const QString& sessionId, const domain::conversation::Message& message);
        void runCompleted(const QString& sessionId);
        void runFailed(const QString& sessionId, const domain::llm::ChatError& error);

    private:
        ports::IAgentRuntime* m_runtime = nullptr;
        domain::service::IModelService* m_modelService = nullptr;
        domain::service::IProjectContextService* m_projectContextService = nullptr;
        llm::mcp::McpManager* m_mcpManager = nullptr;
        ::agent::runtime::AgentContextBuilder m_contextBuilder;
    };

} // namespace application::usecase::agent
