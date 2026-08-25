# Agent 运行时

项目 Agent 与普通聊天分离。普通聊天由 `SendMessageUseCase` 处理纯文本流；项目任务由 `RunAgentUseCase` 和 `AgentRuntime` 执行模型—工具循环。

```text
WorkViewModel
  → RunAgentUseCase
  → AgentRuntime
    → IChatModelGateway
    → ToolRegistry
    → Conversation / Checkpoint Repository
```

## 运行状态

`AgentRunState` 用于向 UI 投影当前任务。主要状态包括：

- `Preparing`：组装上下文和用户消息。
- `CallingModel`：流式请求模型。
- `WaitingPermission`：等待用户批准敏感工具。
- `ExecutingTool`：执行工具并收集结果。
- `Continuing`：将结果回传模型进行下一轮推理。
- `Completed`、`Failed`、`Cancelled`、`Suspended`：终态或可恢复状态。

Agent 默认最多连续执行 8 轮工具调用，可由 `AgentPolicy::maxToolRounds` 调整。

## 项目上下文

`RunAgentUseCase` 基于工作区组装系统提示词，包含项目根目录、`AGENTS.md` 和已启用 Skill。项目 MCP 配置不会作为原始 JSON 拼入提示词，而是在 Runtime 中注册为工具。

## 配置与恢复

Agent 配置可持久化模型、已启用工具、Skill 和 MCP 服务。运行时会持久化 checkpoint；在待工具或待授权状态中断后，可以恢复调用并重新进入权限流程。

## 取消与权限

`CancelAgentRunUseCase` 会取消模型请求，并把运行级取消令牌传播给工具。需要确认的工具会发出 `permissionRequested`，由 `WorkViewModel::grantPermission()` 批准或拒绝。
