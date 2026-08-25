#pragma once

#include "application/ports/ITool.h"
#include "application/ports/IProcessTaskRuntime.h"
#include <memory>

namespace agent::tool::builtin {

    /**
     * @brief 检查并增量读取后台进程任务输出与状态的内置工具
     */
    class CheckTaskTool : public application::ports::ITool {
    public:
        explicit CheckTaskTool(
            std::shared_ptr<application::ports::IProcessTaskRuntime> taskRuntime = nullptr
        );

        domain::agent::ToolDefinition definition() const override;
        application::ports::ToolExecutionTraits traits() const override;
        QList<domain::agent::ToolPermission> permissions(const domain::agent::ToolCall& call) const override;

        std::unique_ptr<application::ports::IToolOperation> execute(
            const domain::agent::ToolCall& call,
            const application::ports::ToolExecutionContext& context
        ) override;

    private:
        std::shared_ptr<application::ports::IProcessTaskRuntime> m_taskRuntime;
    };

} // namespace agent::tool::builtin
