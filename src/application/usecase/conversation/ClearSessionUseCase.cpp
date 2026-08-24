#include "ClearSessionUseCase.h"
#include "domain/service/IConversationService.h"

namespace application::usecase::conversation {
ClearSessionUseCase::ClearSessionUseCase(domain::service::IConversationService* service, QObject* parent)
    : QObject(parent), m_service(service) {}

void ClearSessionUseCase::execute(const QString& sessionId) {
    if (m_service && !sessionId.isEmpty()) m_service->saveMessages(sessionId, {});
}
} // namespace application::usecase::conversation
