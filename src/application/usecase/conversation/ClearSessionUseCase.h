#pragma once

#include <QObject>
#include <QString>

namespace domain::service { class IConversationService; }

namespace application::usecase::conversation {
class ClearSessionUseCase final : public QObject {
    Q_OBJECT
public:
    explicit ClearSessionUseCase(domain::service::IConversationService* service, QObject* parent = nullptr);
    void execute(const QString& sessionId);
private:
    domain::service::IConversationService* m_service = nullptr;
};
} // namespace application::usecase::conversation
