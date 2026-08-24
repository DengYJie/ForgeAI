#include "SetSessionTitleUseCase.h"
#include "domain/service/IConversationService.h"
namespace application::usecase::conversation {
SetSessionTitleUseCase::SetSessionTitleUseCase(domain::service::IConversationService* service, QObject* parent)
    : QObject(parent), m_service(service) {}
void SetSessionTitleUseCase::execute(QList<ui::screen::chat::ChatSessionItemData>& sessions,
                                     const QString& id, const QString& title) {
    if (m_service) m_service->setSessionTitle(sessions, id, title);
}
}
