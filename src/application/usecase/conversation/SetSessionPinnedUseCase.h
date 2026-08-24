#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include "ui/screen/chat/ChatSessionListModel.h"
namespace domain::service { class IConversationService; }
namespace application::usecase::conversation {
class SetSessionPinnedUseCase final : public QObject {
    Q_OBJECT
public:
    explicit SetSessionPinnedUseCase(domain::service::IConversationService* service, QObject* parent = nullptr);
    void execute(QList<ui::screen::chat::ChatSessionItemData>& sessions, const QString& sessionId, bool pinned);
private:
    domain::service::IConversationService* m_service = nullptr;
};
}
