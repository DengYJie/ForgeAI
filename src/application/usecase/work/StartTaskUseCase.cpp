#include "StartTaskUseCase.h"

namespace application::usecase::work {
    StartTaskUseCase::StartTaskUseCase(QObject *parent)
        : QObject(parent) {
    }

    void StartTaskUseCase::execute(const QString &task) {
        if (task.trimmed().isEmpty()) return;
        emit taskStarted(task);
    }
} // namespace application::usecase::work
