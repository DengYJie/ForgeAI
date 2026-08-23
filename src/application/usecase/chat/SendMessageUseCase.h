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
     * @brief 发送消息并触发大模型回复生成的业务用例
     * @details 负责编排用户消息实体构建、历史数据存储、调用 IChatService 发起请求，
     *          并通过 Qt Signal 将流式增量与最终结果回传给表现层。
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
         * @param sessionId 归属会话 ID
         * @param message 新建的用户消息领域实体
         */
        void userMessageCreated(const QString &sessionId, const domain::conversation::Message &message);

        /**
         * @brief 收到模型生成的流式增量 Token
         * @param sessionId 归属会话 ID
         * @param token 增量文本分片
         */
        void tokenReceived(const QString &sessionId, const QString &token);

        /**
         * @brief 助手完整回复消息实体构建完成
         * @param sessionId 归属会话 ID
         * @param message 助手的完整回复消息领域实体
         */
        void replyGenerated(const QString &sessionId, const domain::conversation::Message &message);

        /**
         * @brief 本轮对话生成彻底完成
         * @param sessionId 归属会话 ID
         */
        void generationFinished(const QString &sessionId);

        /**
         * @brief 生成过程异常报错通知
         * @param sessionId 归属会话 ID
         * @param errorMessage 错误描述文本
         */
        void generationFailed(const QString &sessionId, const QString &errorMessage);

    private:
        void setupServiceConnections();

        domain::service::IChatService *m_chatService = nullptr;
        domain::service::IConversationService *m_conversationService = nullptr;
    };
} // namespace application::usecase::chat
