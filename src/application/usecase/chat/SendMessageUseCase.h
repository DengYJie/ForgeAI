#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include "domain/conversation/Message.h"
#include "application/ports/IChatModelGateway.h"

namespace domain::service {
    class IConversationService;
    class IModelService;
}

namespace application::usecase::chat {

    /**
     * @brief 发送消息并触发大模型回复生成的业务用例（纯文本/思考流 Chat）
     * @details 负责编排用户消息实体构建、历史数据存储、调用 IChatModelGateway 发起请求，
     *          并将 llm::ChatEvent 流转化为业务事件。
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
        void execute(const QString &sessionId, const QString &text,
                     const QString &providerId = {}, const QString &modelId = {},
                     bool useWebSearch = false, bool useDeepThinking = false, const QString& reasoningEffort = {},
                     const QString& systemPrompt = {});
        
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
        void replyGenerated(const QString &sessionId, const domain::conversation::Message &message);
        void generationFinished(const QString &sessionId);
        void generationFailed(const QString &sessionId, const domain::llm::ChatError &error);

    private Q_SLOTS:
        void onChatEventReceived(const domain::llm::ChatEvent &event);

    private:
        void startRequest(const domain::model::ModelProvider& provider, const domain::llm::ChatRequest& request);
        domain::llm::ChatRequest requestForHistory(const QList<domain::conversation::Message>& history) const;
        domain::conversation::Message makeAssistantMessage() const;
        void saveMessage(const domain::conversation::Message& message);
        void completeGeneration();

        ports::IChatModelGateway *m_chatGateway = nullptr;
        domain::service::IConversationService *m_conversationService = nullptr;
        domain::service::IModelService *m_modelService = nullptr;
        
        ports::IChatOperation *m_currentOp = nullptr;
        QString m_currentSessionId;
        QString m_currentOperationId;
        
        QString m_replyBuffer;
        QString m_thoughtBuffer;
        domain::model::ModelProvider m_currentProvider;
        domain::llm::ChatRequest m_requestTemplate;
        QString m_systemPrompt;
        QHash<QString, QList<domain::conversation::Message>> m_transientHistories;
    };

} // namespace application::usecase::chat
