#pragma once

#include <QObject>
#include <QList>
#include "domain/conversation/Message.h"

namespace domain::service {
    class IConversationService;
}

namespace application::usecase::conversation {
    /**
     * @brief 加载指定会话消息历史详情用例
     */
    class LoadSessionDetailUseCase : public QObject {
        Q_OBJECT

    public:
        explicit LoadSessionDetailUseCase(
            domain::service::IConversationService *conversationService,
            QObject *parent = nullptr
        );

        ~LoadSessionDetailUseCase() override = default;

        QList<domain::conversation::Message> execute(const QString &sessionId);

    private:
        domain::service::IConversationService *m_conversationService = nullptr;
    };
} // namespace application::usecase::conversation
