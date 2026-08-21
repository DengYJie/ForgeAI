#pragma once

#include "ui/base/BaseViewModel.h"
#include <QString>

namespace ui::screen::work {
    struct WorkState {
        QString currentTask;
        bool isProcessing = false;
        QString statusMessage;

        bool operator==(const WorkState &other) const = default;
    };

    class WorkViewModel : public BaseViewModel<WorkViewModel, WorkState> {
        Q_OBJECT

    public:
        explicit WorkViewModel(QObject *parent = nullptr);

        ~WorkViewModel() override;

        void startTask(const QString &task);

        void cancelTask();

    Q_SIGNALS:
        void stateChanged(const ui::screen::work::WorkState &state);

    protected:
        void emitStateChanged() override;
    };
} // namespace ui::screen::work
