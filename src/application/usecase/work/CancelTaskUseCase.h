#pragma once

#include <QObject>

namespace application::usecase::work {
    /**
     * @brief 取消工作流任务用例
     */
    class CancelTaskUseCase : public QObject {
        Q_OBJECT

    public:
        explicit CancelTaskUseCase(QObject *parent = nullptr);
        ~CancelTaskUseCase() override = default;

        /**
         * @brief 执行任务取消
         */
        void execute();

    Q_SIGNALS:
        void taskCancelled();
    };
} // namespace application::usecase::work
