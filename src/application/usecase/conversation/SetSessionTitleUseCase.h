#pragma once

#include <QObject>
#include <QList>
#include "ui/screen/chat/ChatSessionListModel.h"
namespace domain::service { class IConversationService; }
namespace application::usecase::conversation {
class SetSessionTitleUseCase final : public QObject {
    Q_OBJECT
public:
    explicit SetSessionTitleUseCase(domain::service::IConversationService* service, QObject* parent = nullptr);
    void execute(QList<ui::screen::chat::ChatSessionItemData>& sessions, const QString& id, const QString& title);
private:
    domain::service::IConversationService* m_service = nullptr;
};
}
