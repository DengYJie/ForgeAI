#pragma once

#include <QUuid>
#include <QString>
#include <QList>
#include "domain/agent/ToolExecution.h"

namespace domain::agent {

    /**
     * @brief Agent 运行时生命周期状态
     */
    enum class AgentRunStatus {
        Idle,           ///< 空闲状态
        Preparing,      ///< 准备上下文中
        CallingModel,   ///< 正在调用模型生成回复或工具调用指令
        WaitingPermission, ///< 等待用户授权敏感工具操作 (HITL)
        WaitingTool,    ///< 等待工具就绪
        ExecutingTool,  ///< 正在执行本地或 MCP 工具
        PersistingToolResult, ///< 工具执行完毕，正在将结果保序落盘与保存快照
        Continuing,     ///< 工具结果已持久化，正在发起下一轮模型推理
        Completed,      ///< 任务成功完成
        Failed,         ///< 任务执行失败
        Cancelled,      ///< 用户或系统取消任务
        Suspended       ///< 任务挂起（等待断点恢复）
    };

    /**
     * @brief Agent 运行时状态实体
     */
    struct AgentRunState {
        QUuid runId;
        QString conversationId;
        AgentRunStatus status = AgentRunStatus::Idle;

        int round = 0;
        QList<domain::agent::ToolCall> pendingCalls;
        QList<domain::agent::ToolResult> results;
        QString errorMessage;

        bool operator==(const AgentRunState &other) const = default;
    };

} // namespace domain::agent
