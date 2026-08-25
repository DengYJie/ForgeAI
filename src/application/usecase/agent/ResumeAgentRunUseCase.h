#pragma once

#include <QObject>
#include "application/ports/IAgentRuntime.h"
#include "agent/runtime/AgentRunContext.h"

namespace application::usecase::agent {

    /**
     * @brief 恢复 Agent 执行的业务用例
     */
    class ResumeAgentRunUseCase : public QObject {
        Q_OBJECT
    public:
        explicit ResumeAgentRunUseCase(ports::IAgentRuntime* runtime, QObject* parent = nullptr)
            : QObject(parent), m_runtime(runtime) {}
        ~ResumeAgentRunUseCase() override = default;

        void execute(const ::agent::runtime::AgentRunContext& context) {
            if (m_runtime) {
                m_runtime->resumeRun(context);
            }
        }

    private:
        ports::IAgentRuntime* m_runtime = nullptr;
    };

} // namespace application::usecase::agent
