#pragma once

#include <QObject>
#include <QString>
#include <QUuid>
#include "domain/agent/AgentRunState.h"
#include "domain/agent/ToolPermission.h"
#include "domain/conversation/Message.h"
#include "domain/llm/ChatError.h"
#include "agent/runtime/AgentRunContext.h"

namespace application::ports {

    /**
     * @brief Agent 运行时接口
     */
    class IAgentRuntime : public QObject {
        Q_OBJECT
    public:
        using QObject::QObject;
        ~IAgentRuntime() override = default;

        /**
         * @brief 启动 Agent 执行循环
         */
        virtual void startRun(const agent::runtime::AgentRunContext& context, const QString& prompt) = 0;

        /**
         * @brief 取消当前运行中的任务
         */
        virtual void cancelRun() = 0;

        /**
         * @brief 挂起当前任务
         */
        virtual void suspendRun() = 0;

        /**
         * @brief 恢复挂起的任务
         */
        virtual void resumeRun(const agent::runtime::AgentRunContext& context) = 0;

        /**
         * @brief 用户确认或拒绝某项敏感工具执行权限
         */
        virtual void grantPermission(const QString& sessionId, const QString& toolCallId, bool granted) = 0;

        /**
         * @brief 查询是否处于运行状态
         */
        virtual bool isRunning() const = 0;

        /**
         * @brief 获取当前运行时状态快照
         */
        virtual domain::agent::AgentRunState currentState() const = 0;

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
    };

} // namespace application::ports
