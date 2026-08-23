#pragma once

#include <QObject>
#include <QList>
#include "ui/screen/chat/ChatSessionListModel.h"

namespace domain::service {
    class IConversationService;
}

namespace application::usecase::conversation {
    /**
     * @brief 加载会话列表用例
     */
    class LoadSessionsUseCase : public QObject {
        Q_OBJECT

    public:
        explicit LoadSessionsUseCase(
            domain::service::IConversationService *conversationService,
            QObject *parent = nullptr
        );

        ~LoadSessionsUseCase() override = default;

        QList<ui::screen::chat::ChatSessionItemData> execute();

    private:
        domain::service::IConversationService *m_conversationService = nullptr;
    };
} // namespace application::usecase::conversation
