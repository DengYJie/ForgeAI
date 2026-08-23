#include "CreateSessionUseCase.h"
#include "domain/service/IConversationService.h"

namespace application::usecase::conversation {
    CreateSessionUseCase::CreateSessionUseCase(
        domain::service::IConversationService *conversationService,
        QObject *parent
    ) : QObject(parent), m_conversationService(conversationService) {
    }

    QString CreateSessionUseCase::execute(
        QList<ui::screen::chat::ChatSessionItemData> &sessions,
        const QString &currentSessionId
    ) {
        if (!m_conversationService) {
            return currentSessionId;
        }
        return m_conversationService->reuseOrCreateSession(sessions, currentSessionId);
    }
} // namespace application::usecase::conversation
