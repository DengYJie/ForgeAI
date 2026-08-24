#include "SetSessionArchivedUseCase.h"
#include "domain/service/IConversationService.h"

namespace application::usecase::conversation {
SetSessionArchivedUseCase::SetSessionArchivedUseCase(domain::service::IConversationService* conversationService, QObject* parent)
    : QObject(parent), m_conversationService(conversationService) {}

void SetSessionArchivedUseCase::execute(QList<ui::screen::chat::ChatSessionItemData>& sessions,
                                        const QString& sessionId, bool archived) {
    if (m_conversationService) m_conversationService->setSessionArchived(sessions, sessionId, archived);
}
} // namespace application::usecase::conversation
