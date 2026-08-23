#pragma once

#include <QObject>

namespace domain::service {
    class IChatService;
}

namespace application::usecase::chat {
    /**
     * @brief 中止当前生成任务业务用例
     * @details 负责协调 IChatService 立即中止指定/当前正在进行的异步生成流。
     */
    class StopGenerationUseCase : public QObject {
        Q_OBJECT

    public:
        explicit StopGenerationUseCase(
            domain::service::IChatService *chatService,
            QObject *parent = nullptr
        );

        ~StopGenerationUseCase() override = default;

        /**
         * @brief 执行终止当前生成任务
         */
        void execute();

    private:
        domain::service::IChatService *m_chatService = nullptr;
    };
} // namespace application::usecase::chat
