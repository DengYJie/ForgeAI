#pragma once

#include "ui/base/BaseViewModel.h"
#include "application/usecase/work/WorkUseCases.h"
#include <QString>

namespace ui::screen::work {
    struct WorkState {
        QString currentTask;
        bool isProcessing = false;
        QString statusMessage;

        bool operator==(const WorkState &other) const = default;
    };

    /**
     * @brief 工作流界面的 ViewModel，负责任务派发与状态响应
     */
    class WorkViewModel : public BaseViewModel<WorkViewModel, WorkState> {
        Q_OBJECT

    public:
        explicit WorkViewModel(
            const application::usecase::work::WorkUseCases &useCases = {},
            QObject *parent = nullptr
        );

        ~WorkViewModel() override;

        /**
         * @brief 启动工作流任务
         */
        void startTask(const QString &task);

        /**
         * @brief 取消当前工作流任务
         */
        void cancelTask();

    Q_SIGNALS:
        void stateChanged(const ui::screen::work::WorkState &state);

    protected:
        void emitStateChanged() override;

    private:
        void setupUseCaseConnections();

        application::usecase::work::WorkUseCases m_useCases;
    };
} // namespace ui::screen::work
