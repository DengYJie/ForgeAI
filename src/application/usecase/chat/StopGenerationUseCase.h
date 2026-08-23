#pragma once

#include <QObject>

namespace application::usecase::chat {

    class SendMessageUseCase;

    /**
     * @brief 中断当前正在进行的大模型生成任务的业务用例
     */
    class StopGenerationUseCase : public QObject {
        Q_OBJECT

    public:
        explicit StopGenerationUseCase(
            SendMessageUseCase *sendMessageUseCase,
            QObject *parent = nullptr
        );

        ~StopGenerationUseCase() override = default;

        /**
         * @brief 执行中断生成业务逻辑
         */
        void execute();

    private:
        SendMessageUseCase *m_sendMessageUseCase;
    };

} // namespace application::usecase::chat
