#pragma once

#include "application/ports/IAgentRuntime.h"
#include "application/ports/IChatModelGateway.h"
#include "domain/service/IConversationService.h"
#include "domain/repository/IAgentCheckpointRepository.h"
#include "domain/agent/ToolPermission.h"
#include "agent/tool/ToolRegistry.h"
#include <QMap>
#include <QHash>
#include <vector>
#include <memory>

#include "application/ports/IProcessTaskRuntime.h"

namespace agent::runtime {

    /**
     * @brief Agent 核心运行时引擎
     * @details 负责 Agent 多轮工具循环、ToolCall-Result 累加调度、状态机转换、权限确认 (HITL) 与断点恢复。
     */
    class AgentRuntime final : public application::ports::IAgentRuntime {
        Q_OBJECT
    public:
        explicit AgentRuntime(
            application::ports::IChatModelGateway* chatGateway,
            domain::service::IConversationService* conversationService,
            agent::tool::ToolRegistry* toolRegistry,
            domain::repository::IAgentCheckpointRepository* checkpointRepo,
            QObject* parent
        );

        explicit AgentRuntime(
            application::ports::IChatModelGateway* chatGateway,
            domain::service::IConversationService* conversationService,
            agent::tool::ToolRegistry* toolRegistry,
            domain::repository::IAgentCheckpointRepository* checkpointRepo = nullptr,
            std::shared_ptr<application::ports::IProcessTaskRuntime> taskRuntime = nullptr,
            QObject* parent = nullptr
        );
        ~AgentRuntime() override;

        void startRun(const AgentRunContext& context, const QString& prompt) override;
        void cancelRun() override;
        void suspendRun() override;
        void resumeRun(const AgentRunContext& context) override;
        void grantPermission(
            const QString& sessionId,
            const QString& toolCallId,
            bool granted,
            domain::agent::PermissionScope scope = domain::agent::PermissionScope::Once
        ) override;

        bool isRunning() const override;
        domain::agent::AgentRunState currentState() const override;

    private Q_SLOTS:
        void onChatEventReceived(const domain::llm::ChatEvent& event);
        void onTimeout();

    private:
        void setState(domain::agent::AgentRunStatus status, const QString& errorMessage = {});
        void startNextModelRequest();
        domain::llm::ChatRequest buildChatRequest(const QList<domain::conversation::Message>& history) const;
        domain::conversation::Message makeAssistantMessage() const;
        void saveMessage(const domain::conversation::Message& message);
        void cleanupCurrentOp();
        void saveCheckpoint();
        void processExecutableToolCalls();
        void executeNextBatch();
        void onToolOperationFinished(const QString& toolCallId, const domain::agent::ToolResult& result);
        void finishToolExecutionRound();

        application::ports::IChatModelGateway* m_chatGateway = nullptr;
        domain::service::IConversationService* m_conversationService = nullptr;
        agent::tool::ToolRegistry* m_toolRegistry = nullptr;
        domain::repository::IAgentCheckpointRepository* m_checkpointRepo = nullptr;

        AgentRunContext m_context;
        domain::agent::AgentRunState m_state;
        application::ports::IChatOperation* m_currentOp = nullptr;
        class QTimer* m_timeoutTimer = nullptr;

        QString m_replyBuffer;
        QString m_thoughtBuffer;
        QUuid m_currentAssistantMessageId;
        QMap<QString, domain::agent::ToolCall> m_activeToolCalls;
        QList<domain::agent::ToolResult> m_pendingToolResults;
        QMap<QString, std::pair<domain::agent::ToolCall, domain::agent::ToolPermission>> m_pendingPermissions;
        QSet<QString> m_runApprovedTools;
        QMap<QUuid, QSet<QString>> m_projectApprovedTools;
        QSet<QString> m_globalApprovedTools;
        QList<QList<domain::agent::ToolCall>> m_pendingBatches;
        std::vector<std::unique_ptr<application::ports::IToolOperation>> m_activeOperations;
        QHash<QString, QList<domain::conversation::Message>> m_transientHistories;

        application::ports::CancellationToken m_runCancellationToken;
        std::shared_ptr<application::ports::IProcessTaskRuntime> m_taskRuntime;
    };

} // namespace agent::runtime
