#pragma once

#include <QObject>
#include <QList>
#include "ui/screen/chat/ChatSessionListModel.h"

namespace domain::service { class IConversationService; }

namespace application::usecase::conversation {
class SetSessionArchivedUseCase final : public QObject {
    Q_OBJECT
public:
    explicit SetSessionArchivedUseCase(domain::service::IConversationService* conversationService, QObject* parent = nullptr);
    void execute(QList<ui::screen::chat::ChatSessionItemData>& sessions, const QString& sessionId, bool archived);
private:
    domain::service::IConversationService* m_conversationService = nullptr;
};
} // namespace application::usecase::conversation
