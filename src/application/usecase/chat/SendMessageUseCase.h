#pragma once

#include <QObject>
#include <QString>
#include "domain/conversation/Message.h"
#include "application/ports/IChatModelGateway.h"

namespace domain::service {
    class IConversationService;
    class IModelService;
}

namespace application::usecase::chat {

    /**
     * @brief 发送消息并触发大模型回复生成的业务用例
     * @details 负责编排用户消息实体构建、历史数据存储、调用 IChatModelGateway 发起真实请求，
     *          并将 llm::ChatEvent 流转化为上层需要的业务事件。
     */
    class SendMessageUseCase : public QObject {
        Q_OBJECT

    public:
        explicit SendMessageUseCase(
            ports::IChatModelGateway *chatGateway,
            domain::service::IConversationService *conversationService,
            domain::service::IModelService *modelService,
            QObject *parent = nullptr
        );

        ~SendMessageUseCase() override;

        /**
         * @brief 执行发送提问业务逻辑
         */
        void execute(const QString &sessionId, const QString &text);
        
        /**
         * @brief 取消当前的生成请求
         */
        void cancelCurrent();
        
        /**
         * @brief 是否正在生成中
         */
        bool isGenerating() const;

    Q_SIGNALS:
        void userMessageCreated(const QString &sessionId, const domain::conversation::Message &message);
        void tokenReceived(const QString &sessionId, const QString &token);
        void thoughtReceived(const QString &sessionId, const QString &thought);
        void toolCallReceived(const QString &sessionId, const domain::agent::ToolCall &toolCall);
        void replyGenerated(const QString &sessionId, const domain::conversation::Message &message);
        void generationFinished(const QString &sessionId);
        void generationFailed(const QString &sessionId, const domain::llm::ChatError &error);

    private Q_SLOTS:
        void onChatEventReceived(const domain::llm::ChatEvent &event);

    private:
        ports::IChatModelGateway *m_chatGateway;
        domain::service::IConversationService *m_conversationService;
        domain::service::IModelService *m_modelService;
        
        ports::IChatOperation *m_currentOp = nullptr;
        QString m_currentSessionId;
        
        QString m_replyBuffer;
        QString m_thoughtBuffer;
        QMap<QString, domain::agent::ToolCall> m_activeToolCalls;
    };

} // namespace application::usecase::chat
