# Agent Tools

Agent 工具体系由 `ITool`、`IToolProvider` 和 `ToolRegistry` 构成。模型看到的是 `ToolDefinition`；实际执行由 `ToolRegistry` 根据工具名路由。

```text
BuiltinToolProvider / McpToolProvider
  → ToolRegistry
  → AgentRuntime
  → ToolCall / ToolResult
```

## 内置工具

当前内置文件工具包括：

- `list_files`
- `read_file`
- `search_text`
- `write_file`

它们使用 `WorkspaceFileSystem` 限制在项目根目录内，拒绝路径穿越和工作区外的符号链接目标；搜索和列表操作会忽略 `.git`、`node_modules`、`build` 等目录。

## 工具权限与白名单

每个工具声明所需权限，例如只读、写工作区、执行进程或外部服务。`AgentPolicy` 可允许、拒绝或要求用户批准。Agent 配置中的 `enabledTools` 同时决定：

1. 哪些工具 Schema 会发送给模型；
2. 哪些模型调用可被 Runtime 实际执行。

## 并发、超时与取消

线程安全工具可并行执行；依赖 Qt QObject 或 MCP 会话的工具在主线程串行执行。`ToolExecutionContext` 携带超时和 `CancellationToken`：运行取消、工具超时和父任务取消都会传播到合作式工具。

工具应定期检查 `context.cancellationToken.isCanceled()`，并尽快返回错误结果。新增工具时应明确 `isThreadSafe()`、权限、输入 JSON Schema 与取消行为。

## MCP 工具

MCP 工具由 `McpToolProvider` 动态提供，命名格式为 `mcp::<server-id>::<tool-name>`。调用时会映射回远端服务的原始工具名。
