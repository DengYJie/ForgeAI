#pragma once

#include "ui/base/BaseViewModel.h"
#include "application/usecase/work/WorkUseCases.h"
#include <QString>
#include <QList>
#include "domain/conversation/Message.h"
namespace domain::service { class IProjectContextService; }
namespace services::agent { class AgentToolService; }

namespace ui::screen::work {
    struct WorkState {
        struct ToolEvent {
            QString name;
            QString arguments;
            QString result;
            bool isError = false;
            bool operator==(const ToolEvent&) const = default;
        };
        QString currentTask;
        bool isProcessing = false;
        QString statusMessage;
        QList<ToolEvent> toolEvents;
        QList<domain::conversation::Message> messages;
        QString projectRoot;
        QString projectName;
        int skillCount = 0;
        bool hasAgentsInstructions = false;
        bool hasMcpConfig = false;
        QString agentInstructions;
        QString mcpConfigContent;

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
        void setProjectRoot(const QString& rootPath);

    Q_SIGNALS:
        void stateChanged(const ui::screen::work::WorkState &state);

    protected:
        void emitStateChanged() override;

    private:
        void setupUseCaseConnections();
        QString projectAgentPrompt() const;
        static QString taskTitle(const QString& task);

        application::usecase::work::WorkUseCases m_useCases;
        domain::service::IProjectContextService* m_projectContext = nullptr;
        domain::service::IAgentToolService* m_agentTools = nullptr;
        QString m_agentSessionId;
    };
} // namespace ui::screen::work
