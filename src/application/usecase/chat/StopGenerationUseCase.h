#pragma once

#include <QObject>

namespace domain::service {
    class IChatService;
}

namespace application::usecase::chat {
    /**
     * @brief 中止当前生成任务用例
     */
    class StopGenerationUseCase : public QObject {
        Q_OBJECT

    public:
        explicit StopGenerationUseCase(
            domain::service::IChatService *chatService,
            QObject *parent = nullptr
        );

        ~StopGenerationUseCase() override = default;

        void execute();

    private:
        domain::service::IChatService *m_chatService = nullptr;
    };
} // namespace application::usecase::chat
