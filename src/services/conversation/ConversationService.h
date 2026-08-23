#pragma once

#include "domain/service/IConversationService.h"
#include <QMap>

namespace domain::repository {
    class IConversationRepository;
}

namespace services::conversation {
    /**
     * @brief 会话管理服务实现（承载会话复用、自动降级与数据缓存）
     */
    class ConversationService : public domain::service::IConversationService {
        Q_OBJECT

    public:
        explicit ConversationService(
            domain::repository::IConversationRepository *conversationRepo = nullptr,
            QObject *parent = nullptr
        );

        ~ConversationService() override = default;

        QList<ui::screen::chat::ChatSessionItemData> loadSessions() override;

        QList<domain::conversation::Message> loadMessages(const QString &sessionId) override;

        void saveMessages(const QString &sessionId, const QList<domain::conversation::Message> &messages) override;

        QString reuseOrCreateSession(
            QList<ui::screen::chat::ChatSessionItemData> &sessions,
            const QString &currentSessionId
        ) override;

        QString deleteSession(
            QList<ui::screen::chat::ChatSessionItemData> &sessions,
            const QString &sessionId,
            const QString &currentSessionId
        ) override;

    private:
        domain::repository::IConversationRepository *m_conversationRepo = nullptr;
        QMap<QString, QList<domain::conversation::Message>> m_memoryMessages;
    };
} // namespace services::conversation
