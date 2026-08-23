#include "DeleteSessionUseCase.h"
#include "domain/service/IConversationService.h"

namespace application::usecase::conversation {
    DeleteSessionUseCase::DeleteSessionUseCase(
        domain::service::IConversationService *conversationService,
        QObject *parent
    ) : QObject(parent), m_conversationService(conversationService) {
    }

    QString DeleteSessionUseCase::execute(
        QList<ui::screen::chat::ChatSessionItemData> &sessions,
        const QString &sessionId,
        const QString &currentSessionId
    ) {
        if (!m_conversationService) {
            return currentSessionId;
        }
        return m_conversationService->deleteSession(sessions, sessionId, currentSessionId);
    }
} // namespace application::usecase::conversation
