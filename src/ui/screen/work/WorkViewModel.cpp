#include "WorkViewModel.h"

namespace ui::screen::work {
    WorkViewModel::WorkViewModel(
        const application::usecase::work::WorkUseCases &useCases,
        QObject *parent
    ) : BaseViewModel<WorkViewModel, WorkState>(parent),
        m_useCases(useCases) {
        setupUseCaseConnections();
    }

    WorkViewModel::~WorkViewModel() = default;

    void WorkViewModel::setupUseCaseConnections() {
        if (m_useCases.startTask) {
            connect(m_useCases.startTask, &application::usecase::work::StartTaskUseCase::taskStarted,
                    this, [this](const QString &task) {
                updateState([task](WorkState &s) {
                    s.currentTask = task;
                    s.isProcessing = true;
                    s.statusMessage = QStringLiteral("工作流执行中: ") + task;
                });
            });

            connect(m_useCases.startTask, &application::usecase::work::StartTaskUseCase::taskCompleted,
                    this, [this](const QString &task) {
                updateState([task](WorkState &s) {
                    s.isProcessing = false;
                    s.statusMessage = QStringLiteral("工作流已完成: ") + task;
                });
            });
        }

        if (m_useCases.cancelTask) {
            connect(m_useCases.cancelTask, &application::usecase::work::CancelTaskUseCase::taskCancelled,
                    this, [this]() {
                updateState([](WorkState &s) {
                    s.isProcessing = false;
                    s.statusMessage = QStringLiteral("工作流已取消");
                });
            });
        }
    }

    void WorkViewModel::startTask(const QString &task) {
        if (task.trimmed().isEmpty()) return;

        if (m_useCases.startTask) {
            m_useCases.startTask->execute(task);
        } else {
            updateState([task](WorkState &s) {
                s.currentTask = task;
                s.isProcessing = true;
                s.statusMessage = QStringLiteral("工作流执行中...");
            });
        }
    }

    void WorkViewModel::cancelTask() {
        if (m_useCases.cancelTask) {
            m_useCases.cancelTask->execute();
        } else {
            updateState([](WorkState &s) {
                s.isProcessing = false;
                s.statusMessage = QStringLiteral("工作流已取消");
            });
        }
    }

    void WorkViewModel::emitStateChanged() {
        Q_EMIT stateChanged(m_state);
    }
} // namespace ui::screen::work
