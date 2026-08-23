#include "CancelTaskUseCase.h"

namespace application::usecase::work {
    CancelTaskUseCase::CancelTaskUseCase(QObject *parent)
        : QObject(parent) {
    }

    void CancelTaskUseCase::execute() {
        emit taskCancelled();
    }
} // namespace application::usecase::work
