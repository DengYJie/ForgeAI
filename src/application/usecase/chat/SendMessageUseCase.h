#pragma once

#include <QObject>
#include <QString>
#include "domain/conversation/Message.h"

namespace domain::service {
    class IChatService;
    class IConversationService;
}

namespace application::usecase::chat {
    /**
     * @brief 发送消息并触发大模型回复生成的用例
     */
    class SendMessageUseCase : public QObject {
        Q_OBJECT

    public:
        explicit SendMessageUseCase(
            domain::service::IChatService *chatService,
            domain::service::IConversationService *conversationService,
            QObject *parent = nullptr
        );

        ~SendMessageUseCase() override = default;

        /**
         * @brief 执行发送提问业务逻辑
         * @param sessionId 目标会话 ID
         * @param text 用户提问正文
         */
        void execute(const QString &sessionId, const QString &text);

    Q_SIGNALS:
        /**
         * @brief 用户消息实体已成功构建并入库
         */
        void userMessageCreated(const QString &sessionId, const domain::conversation::Message &message);

        /**
         * @brief 收到模型生成的流式增量 Token
         */
        void tokenReceived(const QString &sessionId, const QString &token);

        /**
         * @brief 助手完整回复消息实体构建完成
         */
        void replyGenerated(const QString &sessionId, const domain::conversation::Message &message);

        /**
         * @brief 本轮对话生成彻底完成
         */
        void generationFinished(const QString &sessionId);

        /**
         * @brief 生成失败或异常报错
         */
        void generationFailed(const QString &sessionId, const QString &errorMessage);

    private:
        void setupServiceConnections();

        domain::service::IChatService *m_chatService = nullptr;
        domain::service::IConversationService *m_conversationService = nullptr;
    };
} // namespace application::usecase::chat
