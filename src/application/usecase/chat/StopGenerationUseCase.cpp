#include "StopGenerationUseCase.h"
#include "SendMessageUseCase.h"

namespace application::usecase::chat {

    StopGenerationUseCase::StopGenerationUseCase(
        SendMessageUseCase *sendMessageUseCase,
        QObject *parent
    ) : QObject(parent), m_sendMessageUseCase(sendMessageUseCase) {
    }

    void StopGenerationUseCase::execute() {
        if (m_sendMessageUseCase) {
            m_sendMessageUseCase->cancelCurrent();
        }
    }

} // namespace application::usecase::chat
