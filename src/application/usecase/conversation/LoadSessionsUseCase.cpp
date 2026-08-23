#include "LoadSessionsUseCase.h"
#include "domain/service/IConversationService.h"

namespace application::usecase::conversation {
    LoadSessionsUseCase::LoadSessionsUseCase(
        domain::service::IConversationService *conversationService,
        QObject *parent
    ) : QObject(parent), m_conversationService(conversationService) {
    }

    QList<ui::screen::chat::ChatSessionItemData> LoadSessionsUseCase::execute() {
        if (!m_conversationService) {
            return {};
        }
        return m_conversationService->loadSessions();
    }
} // namespace application::usecase::conversation
