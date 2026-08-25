#pragma once

#include <QObject>
#include "application/ports/IAgentRuntime.h"

namespace application::usecase::agent {

    /**
     * @brief 取消 Agent 执行的业务用例
     */
    class CancelAgentRunUseCase : public QObject {
        Q_OBJECT
    public:
        explicit CancelAgentRunUseCase(ports::IAgentRuntime* runtime, QObject* parent = nullptr)
            : QObject(parent), m_runtime(runtime) {}
        ~CancelAgentRunUseCase() override = default;

        void execute() {
            if (m_runtime) {
                m_runtime->cancelRun();
            }
        }

    private:
        ports::IAgentRuntime* m_runtime = nullptr;
    };

} // namespace application::usecase::agent
