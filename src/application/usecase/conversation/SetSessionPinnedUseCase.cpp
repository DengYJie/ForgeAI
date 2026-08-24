#include "SetSessionPinnedUseCase.h"
#include "domain/service/IConversationService.h"
namespace application::usecase::conversation {
SetSessionPinnedUseCase::SetSessionPinnedUseCase(domain::service::IConversationService* service, QObject* parent) : QObject(parent), m_service(service) {}
void SetSessionPinnedUseCase::execute(QList<ui::screen::chat::ChatSessionItemData>& sessions, const QString& sessionId, bool pinned) {
    if (m_service) m_service->setSessionPinned(sessions, sessionId, pinned);
}
}
