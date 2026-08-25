#pragma once

#include <QList>
#include <QString>
#include <QSet>
#include "domain/agent/ToolExecution.h"
#include "agent/tool/ToolRegistry.h"

namespace agent::runtime {

    /**
     * @brief 工具并发执行调度器
     * @details 根据 ToolExecutionTraits 的并发性、串行隔离键名（concurrencyKey）对一组 ToolCall 进行拓扑分批调度。
     */
    class ToolExecutionScheduler {
    public:
        /**
         * @brief 将待执行工具列表划分为有序批次，同批次内的工具可并发执行，不同批次按序执行
         * @param calls 当前轮次待执行的工具调用列表
         * @param registry 工具注册表（查询 ToolExecutionTraits）
         * @param allowParallel 是否允许并发（由 AgentPolicy.allowParallelToolExecution 决定）
         * @return 分批的工具列表（批次间按顺序执行，批次内可并发）
         */
        static QList<QList<domain::agent::ToolCall>> scheduleBatches(
            const QList<domain::agent::ToolCall>& calls,
            const agent::tool::ToolRegistry* registry,
            bool allowParallel = true
        );
    };

} // namespace agent::runtime
