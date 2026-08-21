#include "WorkViewModel.h"

namespace ui::screen::work {
    WorkViewModel::WorkViewModel(QObject *parent)
        : BaseViewModel<WorkViewModel, WorkState>(parent) {
    }

    WorkViewModel::~WorkViewModel() = default;

    void WorkViewModel::startTask(const QString &task) {
        if (task.trimmed().isEmpty()) return;

        updateState([task](WorkState &s) {
            s.currentTask = task;
            s.isProcessing = true;
            s.statusMessage = QStringLiteral("工作流执行中...");
        });
    }

    void WorkViewModel::cancelTask() {
        updateState([](WorkState &s) {
            s.isProcessing = false;
            s.statusMessage = QStringLiteral("工作流已取消");
        });
    }

    void WorkViewModel::emitStateChanged() {
        Q_EMIT stateChanged(m_state);
    }
} // namespace ui::screen::work
