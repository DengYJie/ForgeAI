#pragma once

#include "ui/base/BaseViewModel.h"
#include "application/usecase/work/WorkUseCases.h"
#include <QString>
#include <QList>
#include <QSet>
#include "domain/conversation/Message.h"
#include "domain/agent/AgentRunState.h"
#include "domain/agent/ToolPermission.h"
#include "ui/screen/chat/ChatSessionListModel.h"
#include "ui/screen/chat/ChatViewModel.h"
#include "domain/project/Project.h"

namespace domain::service { class IProjectContextService; }
namespace domain::service { class IConversationService; }
namespace domain::repository { class IConversationRepository; class IProjectRepository; }

namespace ui::screen::work {

    /**
     * @brief 待处理权限请求项
     */
    struct PendingPermission {
        domain::agent::ToolCall call;
        domain::agent::ToolPermission permission;
        bool operator==(const PendingPermission&) const = default;
    };

    /**
     * @brief Agent 运行时的表现层 UI 状态投影
     */
    struct AgentRunUiState {
        QString currentTaskId;
        domain::agent::AgentRunStatus status = domain::agent::AgentRunStatus::Idle;
        int currentRound = 0;
        int maxRounds = 8;
        QString activeToolName;
        QString statusMessage;
        bool isWaitingPermission = false;
        QList<PendingPermission> pendingPermissions;

        bool operator==(const AgentRunUiState&) const = default;
    };

    struct WorkState {
        struct ToolEvent {
            QString toolCallId;
            QString name;
            QString arguments;
            QString result;
            bool isError = false;
            bool operator==(const ToolEvent&) const = default;
        };
        QString currentTask;
        bool isProcessing = false;
        QString statusMessage;
        AgentRunUiState agentUiState;
        QList<ToolEvent> toolEvents;
        QList<domain::conversation::Message> messages;
        QList<ui::screen::chat::ChatSessionItemData> sessions;
        QList<domain::project::Project> projects;
        QSet<QUuid> pinnedProjects;
        QString currentSessionId;
        QUuid currentProjectId;
        QString projectRoot;
        QString projectName;
        int skillCount = 0;
        bool hasAgentsInstructions = false;
        bool hasMcpConfig = false;
        QString agentInstructions;
        QString mcpConfigContent;

        // 模型与控制
        QString currentModelName = QStringLiteral("选择模型");
        QString currentModelProviderId;
        QString currentModelId;
        QList<ui::screen::chat::ChatModelOption> availableModels;
        bool useWebSearch = false;
        bool useDeepThinking = false;
        QString reasoningEffort;

        bool operator==(const WorkState &other) const = default;
    };

    /**
     * @brief 工作流界面的 ViewModel，负责任务派发与状态响应
     */
    class WorkViewModel : public BaseViewModel<WorkViewModel, WorkState> {
        Q_OBJECT

    public:
        explicit WorkViewModel(
            const application::usecase::work::WorkUseCases &useCases = {},
            domain::service::IProjectContextService* projectContext = nullptr,
            QObject *parent = nullptr
        );

        ~WorkViewModel() override;

        /**
         * @brief 启动工作流任务
         */
        void startTask(const QString &task);

        /**
         * @brief 取消当前工作流任务
         */
        void cancelTask();

        /**
         * @brief 响应用户对敏感工具操作的权限授权
         */
        void grantPermission(
            const QString& toolCallId,
            bool granted,
            domain::agent::PermissionScope scope = domain::agent::PermissionScope::Once,
            const QString& customInput = QString()
        );

        void setProjectRoot(const QString& rootPath);
        void selectProject(const QUuid& projectId);
        void addProject(const QString& rootPath, const QString& displayName = {});
        void removeProject(const QUuid& projectId);
        void renameProject(const QUuid& projectId, const QString& newName);
        void toggleProjectPinned(const QUuid& projectId);
        void archiveProjectSessions(const QUuid& projectId);
        void newSession();
        void loadSession(const QString& sessionId);
        void setSessionPinned(const QString& sessionId, bool pinned);
        void setSessionArchived(const QString& sessionId, bool archived);

        void setModel(const QString &providerId, const QString &modelId);
        void setReasoningEffort(const QString &effort);
        void setWebSearchEnabled(bool enabled);
        void setDeepThinkingEnabled(bool enabled);
        void refreshAvailableModels();

    Q_SIGNALS:
        void stateChanged(const ui::screen::work::WorkState &state);

    protected:
        void emitStateChanged() override;

    private:
        void refreshAvailableModels(WorkState &state);
        void setupUseCaseConnections();
        static QString taskTitle(const QString& task);

        application::usecase::work::WorkUseCases m_useCases;
        domain::service::IProjectContextService* m_projectContext = nullptr;
        QString m_agentSessionId;
        domain::service::IConversationService* m_conversationService = nullptr;
        domain::repository::IConversationRepository* m_conversationRepository = nullptr;
        domain::repository::IProjectRepository* m_projectRepository = nullptr;
        QUuid m_currentProjectId;
    };
} // namespace ui::screen::work
