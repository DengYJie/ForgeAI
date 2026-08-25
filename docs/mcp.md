# MCP 架构

ForgeAI 通过 MCP（Model Context Protocol）接入项目级外部能力。当前实现将配置、运行、协议和展示分离：

```text
.mcp.json / mcp.json
  → McpConfigLoader → McpServerRegistry → McpRuntime
  → McpSession → IMcpTransport → McpClient
  → McpToolProvider / McpResourceProvider / McpPromptProvider
```

## 配置

项目根目录可放置 `.mcp.json` 或 `mcp.json`。服务使用稳定的 `id` 标识；Stdio 服务需要 `command`，HTTP 服务需要 `url`。

```json
{
  "mcpServers": {
    "filesystem": {
      "id": "filesystem",
      "transport": "stdio",
      "command": "npx",
      "args": ["-y", "@modelcontextprotocol/server-filesystem", "."]
    },
    "remote": {
      "id": "remote",
      "transport": "http",
      "url": "https://example.invalid/mcp",
      "headers": {"Authorization": "Bearer <token>"}
    }
  }
}
```

`McpConfigLoader` 会返回结构化诊断；无效服务不会作为可启动配置注册。

## 运行与生命周期

`McpServerRegistry` 仅保存静态配置，`McpRuntime` 管理每个 `McpSession` 的启动、停止、重启和项目卸载。Session 的状态为 `Stopped`、`Starting`、`Initializing`、`Ready`、`Stopping` 或 `Failed`。

切换或删除项目时，Work 页面会卸载该工作区关联的 MCP 服务，避免上个项目的工具泄漏到当前项目。

## 支持的传输

- `stdio`：由 `StdioMcpTransport` 启动子进程并处理换行分隔 JSON-RPC。
- `http`：由 `StreamableHttpMcpTransport` 发送 JSON-RPC POST，并支持 SSE 的 `event:` / `data:` 分帧。

WebSocket 配置类型已预留，但尚未提供 Transport 实现。

## 暴露的能力

工具按 `mcp::<server-id>::<tool-name>` 命名，以避免跨服务重名。Client 同时支持工具、Resource 和 Prompt 的发现与调用。

## 安全

服务启动须通过 `McpServerTrustPolicy`。未信任服务默认拒绝启动；可按服务设置单次批准或永久允许。环境变量若需要展示或记录，应使用 `maskSensitiveEnv()` 脱敏。
