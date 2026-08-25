#pragma once

#include "ToolPermission.h"
#include <QSet>

namespace domain::agent {

    /**
     * @brief Agent 运行策略与安全约束
     */
    struct AgentPolicy {
        int maxToolRounds = 8;                  ///< 最大允许的连续工具调用轮数
        int timeoutMs = 120000;                 ///< 单轮调用超时时间 (ms)
        bool allowParallelToolExecution = true; ///< 是否允许同轮内的并发工具执行

        bool autoApproveReadOnly = true;        ///< 默认自动授权只读操作
        bool autoApproveWriteWorkspace = true;   ///< 工作区写操作策略
        bool autoApproveExecuteProcess = false;  ///< 进程执行默认需确认

        PermissionDecision evaluatePermission(ToolPermissionType type) const {
            switch (type) {
            case ToolPermissionType::ReadOnly:
                return autoApproveReadOnly ? PermissionDecision::Allow : PermissionDecision::AskUser;
            case ToolPermissionType::WriteWorkspace:
                return autoApproveWriteWorkspace ? PermissionDecision::Allow : PermissionDecision::AskUser;
            case ToolPermissionType::ExecuteProcess:
                return autoApproveExecuteProcess ? PermissionDecision::Allow : PermissionDecision::AskUser;
            case ToolPermissionType::Network:
            case ToolPermissionType::ExternalService:
                return PermissionDecision::Allow;
            }
            return PermissionDecision::Allow;
        }

        bool operator==(const AgentPolicy &other) const = default;
    };

} // namespace domain::agent
