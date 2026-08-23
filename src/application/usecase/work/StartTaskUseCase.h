#pragma once

#include <QObject>
#include <QString>

namespace application::usecase::work {
    /**
     * @brief 启动工作流任务用例
     */
    class StartTaskUseCase : public QObject {
        Q_OBJECT

    public:
        explicit StartTaskUseCase(QObject *parent = nullptr);
        ~StartTaskUseCase() override = default;

        /**
         * @brief 执行任务启动
         * @param task 任务描述/命令
         */
        void execute(const QString &task);

    Q_SIGNALS:
        void taskStarted(const QString &task);
        void taskCompleted(const QString &task);
        void taskFailed(const QString &task, const QString &error);
    };
} // namespace application::usecase::work
