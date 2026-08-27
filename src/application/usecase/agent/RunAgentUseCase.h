#pragma once

#include <QObject>
#include <QString>
#include <QUuid>
#include "application/ports/IAgentRuntime.h"
#include "domain/service/IModelService.h"
#include "domain/service/IProjectContextService.h"
#include "domain/repository/IAgentRepository.h"
#include "agent/skill/SkillRegistry.h"
#include "agent/runtime/AgentContextBuilder.h"
#include "application/ports/IProjectRuntimeCoordinator.h"

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
            ports::IProjectRuntimeCoordinator* runtimeCoordinator = nullptr,
            domain::repository::IAgentRepository* agentRepository = nullptr,
            ::agent::skill::SkillRegistry* skillRegistry = nullptr,
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
            const QString& customSystemPrompt = {},
            const QUuid& agentId = {}
        );

        void cancelCurrent();
        void grantPermission(const QString& sessionId, const QString& toolCallId, bool granted);
        bool isRunning() const;
        ports::IAgentRuntime* runtime() const;

    signals:
        void userMessageCreated(const QString& sessionId, const domain::conversation::Message& message);
        void assistantMessageStarted(const QString& sessionId, const domain::conversation::Message& message);
        void stateChanged(const domain::agent::AgentRunState& state);
        void tokenReceived(const QString& sessionId, const QUuid& messageId, const QString& token);
        void thoughtReceived(const QString& sessionId, const QUuid& messageId, const QString& thought);
        void toolCallFinished(const QString& sessionId, const QUuid& messageId, const domain::agent::ToolCall& call);
        void toolResultReady(const QString& sessionId, const QUuid& messageId, const domain::agent::ToolResult& result);
        void permissionRequested(const QString& sessionId, const domain::agent::ToolCall& call, const domain::agent::ToolPermission& permission);
        void replyGenerated(const QString& sessionId, const domain::conversation::Message& message);
        void runCompleted(const QString& sessionId);
        void runFailed(const QString& sessionId, const domain::llm::ChatError& error);

    private:
        ports::IAgentRuntime* m_runtime = nullptr;
        domain::service::IModelService* m_modelService = nullptr;
        domain::service::IProjectContextService* m_projectContextService = nullptr;
        ports::IProjectRuntimeCoordinator* m_runtimeCoordinator = nullptr;
        domain::repository::IAgentRepository* m_agentRepository = nullptr;
        ::agent::skill::SkillRegistry* m_skillRegistry = nullptr;
        ::agent::runtime::AgentContextBuilder m_contextBuilder;
    };

} // namespace application::usecase::agent
