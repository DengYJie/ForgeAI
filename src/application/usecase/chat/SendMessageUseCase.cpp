#include "SendMessageUseCase.h"
#include "domain/service/IChatService.h"
#include "domain/service/IConversationService.h"
#include <QDateTime>
#include <QUuid>

namespace application::usecase::chat {
    SendMessageUseCase::SendMessageUseCase(
        domain::service::IChatService *chatService,
        domain::service::IConversationService *conversationService,
        QObject *parent
    ) : QObject(parent),
        m_chatService(chatService),
        m_conversationService(conversationService) {
        setupServiceConnections();
    }

    void SendMessageUseCase::setupServiceConnections() {
        if (!m_chatService) return;

        connect(m_chatService, &domain::service::IChatService::tokenReceived,
                this, &SendMessageUseCase::tokenReceived);

        connect(m_chatService, &domain::service::IChatService::messageGenerated,
                this, [this](const QString &sessionId, const QString &fullReply) {
            domain::conversation::Message assistantMsg;
            assistantMsg.id = QUuid::createUuid();
            assistantMsg.role = domain::MessageRole::Assistant;
            assistantMsg.status = domain::MessageStatus::Sent;
            assistantMsg.createdAt = QDateTime::currentDateTime();
            assistantMsg.blocks.append(domain::conversation::MessageBlock(
                domain::BlockType::Text,
                domain::conversation::TextBlock{fullReply}
            ));

            if (m_conversationService) {
                auto history = m_conversationService->loadMessages(sessionId);
                history.append(assistantMsg);
                m_conversationService->saveMessages(sessionId, history);
            }

            emit replyGenerated(sessionId, assistantMsg);
        });

        connect(m_chatService, &domain::service::IChatService::generationFinished,
                this, &SendMessageUseCase::generationFinished);

        connect(m_chatService, &domain::service::IChatService::generationFailed,
                this, &SendMessageUseCase::generationFailed);
    }

    void SendMessageUseCase::execute(const QString &sessionId, const QString &text) {
        const QString trimmed = text.trimmed();
        if (trimmed.isEmpty() || sessionId.isEmpty()) return;

        // 1. 构建 User Message 实体
        domain::conversation::Message userMsg;
        userMsg.id = QUuid::createUuid();
        userMsg.role = domain::MessageRole::User;
        userMsg.status = domain::MessageStatus::Sent;
        userMsg.createdAt = QDateTime::currentDateTime();
        userMsg.blocks.append(domain::conversation::MessageBlock(
            domain::BlockType::Text,
            domain::conversation::TextBlock{trimmed}
        ));

        // 2. 存入会话历史
        if (m_conversationService) {
            auto history = m_conversationService->loadMessages(sessionId);
            history.append(userMsg);
            m_conversationService->saveMessages(sessionId, history);
        }

        // 3. 通知上层（ViewModel）
        emit userMessageCreated(sessionId, userMsg);

        // 4. 驱动 ChatService 启动生成
        if (m_chatService) {
            m_chatService->sendMessage(sessionId, trimmed);
        }
    }
} // namespace application::usecase::chat
