#include "StopGenerationUseCase.h"
#include "domain/service/IChatService.h"

namespace application::usecase::chat {
    StopGenerationUseCase::StopGenerationUseCase(
        domain::service::IChatService *chatService,
        QObject *parent
    ) : QObject(parent), m_chatService(chatService) {
    }

    void StopGenerationUseCase::execute() {
        if (m_chatService) {
            m_chatService->stopGeneration();
        }
    }
} // namespace application::usecase::chat
