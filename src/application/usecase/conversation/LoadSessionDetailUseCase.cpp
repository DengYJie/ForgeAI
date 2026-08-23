#include "LoadSessionDetailUseCase.h"
#include "domain/service/IConversationService.h"

namespace application::usecase::conversation {
    LoadSessionDetailUseCase::LoadSessionDetailUseCase(
        domain::service::IConversationService *conversationService,
        QObject *parent
    ) : QObject(parent), m_conversationService(conversationService) {
    }

    QList<domain::conversation::Message> LoadSessionDetailUseCase::execute(const QString &sessionId) {
        if (!m_conversationService || sessionId.isEmpty()) {
            return {};
        }
        return m_conversationService->loadMessages(sessionId);
    }
} // namespace application::usecase::conversation
