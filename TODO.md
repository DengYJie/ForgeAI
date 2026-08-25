# ForgeAI Agent 重构与演进任务清单 (TODO)

## 最终目标架构

```text
Presentation
└── WorkViewModel
        ↓ Intent / State

Application
├── RunAgentUseCase
├── CancelAgentRunUseCase
└── ResumeAgentRunUseCase
        ↓

Agent Runtime
├── AgentRuntime
├── AgentRunContext
├── AgentRunState
├── AgentPolicy
└── AgentContextBuilder
        │
        ├──────── LLM
        │          ↓
        │   IChatModelGateway
        │
        ├──────── Tools
        │          ↓
        │      ToolRegistry
        │      ├── BuiltinToolProvider
        │      └── McpToolProvider
        │
        ├──────── Skills
        │          ↓
        │      SkillRegistry
        │      └── SkillLoader
        │
        └──────── Persistence
                   ↓
          IAgentRepository
          IAgentCheckpointRepository
```

协议层架构：

```text
AgentRuntime
    ↓ ChatRequest
IChatModelGateway
    ↓
ModelProviderService
    ↓
ProtocolAdapter
    ↓
HTTP

HTTP Stream
    ↓
StreamParser
    ↓
ChatEvent
    ↓
AgentRuntime
```

---

## 里程碑与详细任务

### [Milestone 1: 基础设施与工具抽象层]

#### Phase 0：先冻结现有协议层
- [x] 保留 `ChatRequest.tools`
- [x] 保留 `ToolCall / ToolResult`
- [x] 保留 `EventToolCallStarted/Delta/Finished`
- [x] 保留各 Provider Adapter 的工具协议映射
- [x] 增补 Anthropic / Gemini / OpenAI / Responses Tool Calling 测试
- [x] 增加工具调用多轮 round-trip 测试

#### Phase 1：建立 Tool 抽象
- [x] 新建 `ITool` 接口 (`src/application/ports/ITool.h`)
- [x] 新建 `ToolExecutionContext` 结构 (`src/application/ports/ITool.h`)
- [x] 新建 `IToolProvider` 接口 (`src/application/ports/IToolProvider.h`)
- [x] 新建 `ToolRegistry` (`src/agent/tool/ToolRegistry.h/.cpp`)
- [x] 支持 `registerTool()`
- [x] 支持 `registerProvider()`
- [x] 支持 `findTool(name)`
- [x] 支持 `definitions()`
- [x] 支持统一 `execute(call, context)`
- [x] Tool 重名检测与抛错
- [x] 未知 Tool 标准错误返回

#### Phase 3：抽 Workspace Sandbox
- [x] 新建 `WorkspaceFileSystem` (`src/llm/workspace/WorkspaceFileSystem.h/.cpp`)
- [x] 搬迁 canonical path 检查
- [x] 搬迁 Windows case-insensitive 逻辑
- [x] 搬迁 symlink escape 检查
- [x] 加入 `ignorePatterns`
- [x] Build/.git/node_modules 过滤
- [x] Tool 依赖 `WorkspaceFileSystem`

#### Phase 2：拆掉 `AgentToolService`
- [x] 新建 `ReadFileTool` (`src/agent/tool/builtin/ReadFileTool.h/.cpp`)
- [x] 新建 `WriteFileTool` (`src/agent/tool/builtin/WriteFileTool.h/.cpp`)
- [x] 新建 `ListFilesTool` (`src/agent/tool/builtin/ListFilesTool.h/.cpp`)
- [x] 新建 `SearchTextTool` (`src/agent/tool/builtin/SearchTextTool.h/.cpp`)
- [x] 新建 `BuiltinToolProvider` (`src/agent/tool/BuiltinToolProvider.h/.cpp`)
- [x] 迁移后废弃/删除 `AgentToolService` 与 `IAgentToolService`

---

### 里程碑 2：Agent 核心运行时与 UseCase 拆分

#### Phase 4：建立真正 Agent Runtime
- [x] 新建 `AgentRunStatus` (`src/domain/agent/AgentRunState.h`)
- [x] 新建 `AgentRunState` (`src/domain/agent/AgentRunState.h`)
- [x] 新建 `AgentPolicy` (`src/domain/agent/AgentPolicy.h`)
- [x] 新建 `AgentRunContext` (`src/agent/runtime/AgentRunContext.h`)
- [x] 新建 `IAgentRuntime` 接口 (`src/application/ports/IAgentRuntime.h`)
- [x] 新建 `AgentRuntime` (`src/agent/runtime/AgentRuntime.h/.cpp`)
- [x] 迁移多轮循环逻辑 (`maxToolRounds`, `timeoutMs`)
- [x] 接入 `ToolRegistry` 统一执行工具
- [x] 累加 `ToolCallBlock` 与 `ToolResultBlock`
- [x] 抽象 Tool 调度机制

#### Phase 5：拆分 UseCase
- [x] `SendMessageUseCase` 纯化为普通 Chat 业务
- [x] 移除 `SendMessageUseCase` 中的 Tool 循环机制与 Tool 依赖
- [x] 新建 `RunAgentUseCase` (`src/application/usecase/agent/RunAgentUseCase.h/.cpp`)
- [x] 新建 `CancelAgentRunUseCase` (`src/application/usecase/agent/CancelAgentRunUseCase.h/.cpp`)
- [x] 新建 `ResumeAgentRunUseCase` (`src/application/usecase/agent/ResumeAgentRunUseCase.h/.cpp`)
- [x] 新建 `AgentUseCases` 容器 (`src/application/usecase/agent/AgentUseCases.h`)

#### Phase 6：重构 Work 界面对接
- [x] 重构 `WorkUseCases` (`src/application/usecase/work/WorkUseCases.h`)
- [x] 重构 `WorkViewModel` 对接 `RunAgentUseCase` 与 `CancelAgentRunUseCase`
- [x] 移除 `WorkViewModel` 内 `dynamic_cast<AgentToolService*>`
- [x] 移除 `WorkViewModel` 改变工具沙箱状态的逻辑
- [x] 更新 `ApplicationContext` 注入新的 Agent Runtime 与 UseCases
- [x] `WorkViewModel` 仅维护 UDF 状态 (projects, sessions, messages, currentProject, agentRun status, tool UI events)

---

### [Milestone 3: 提示词工程、Skill 体系与上下文收口]

#### Phase 7：建立 AgentContextBuilder
- [x] 新建 `AgentContextBuilder` (`src/agent/runtime/AgentContextBuilder.h/.cpp`)
- [x] 组合 Agent.systemPrompt, Project rules, AGENTS.md, selected Skills, runtime metadata
- [x] Prompt composition 从 ViewModel 移出
- [x] MCP JSON 严禁拼入 system prompt

#### Phase 8：正式实现 Skill
- [x] 升级 `Skill` 领域实体 (`src/domain/agent/Skill.h`)
- [x] 新建 `SkillLoader` (`src/agent/skill/SkillLoader.h/.cpp`)
- [x] 新建 `SkillRegistry` (`src/agent/skill/SkillRegistry.h/.cpp`)
- [x] 扫描 `.agents/skills/*/SKILL.md` 与 `.skills`
- [x] 解析 metadata / frontmatter
- [x] Skill 去重
- [x] Skill 启用/禁用过滤
- [x] `ProjectContextService` 接入 `SkillLoader` 静态解析与发现

#### Phase 12：ProjectContextService 收口
- [x] `ProjectContextService` 纯化为静态扫描与发现
- [x] `SkillRegistry` 接管 Skill 生命周期
- [x] `McpManager` 接管 MCP 生命周期
- [x] `AgentContextBuilder` 接管 Agent 上下文组装
- [x] `ProjectContext` 移除 raw MCP JSON 提示词注入

---

### [Milestone 4: 持久化仓储与 MCP Core]

#### Phase 9：Agent 配置持久化
- [x] 新建 `IAgentRepository` (`src/domain/repository/IAgentRepository.h`)
- [x] 新建 `SqliteAgentRepository` (`src/data/repository/SqliteAgentRepository.h/.cpp`)
- [x] 支持 `getAgent(id)`
- [x] 支持 `getAllAgents()`
- [x] 支持 `saveAgent()` (使用 `SqlHelper`)
- [x] 支持 `deleteAgent()`
- [x] Agent 实体支持 `enabledTools`, `enabledSkills`

#### Phase 10：实现 MCP Core
- [x] 新建 `McpServerConfig` (`src/llm/mcp/McpServerConfig.h`)
- [x] 新建 `IMcpTransport` (`src/llm/mcp/IMcpTransport.h`)
- [x] 新建 `StdioMcpTransport` (`src/llm/mcp/StdioMcpTransport.h/.cpp`)
- [x] 新建 `McpClient` / `McpSession` (`src/llm/mcp/McpClient.h/.cpp`, `McpSession.h/.cpp`)
- [x] JSON-RPC 2.0 请求 / 响应 / 通知
- [x] initialize / initialized 握手
- [x] tools/list
- [x] tools/call
- [x] 生命周期管理 (timeout, cancel, disconnect, error mapping)

#### Phase 11：实现 McpToolProvider
- [x] 新建 `McpToolProvider` (`src/llm/mcp/McpToolProvider.h/.cpp`)
- [x] 实现 `IToolProvider` 接口
- [x] MCP Tool → ToolDefinition 映射
- [x] Tool call → `tools/call` 分发
- [x] MCP result → ToolResult 封装
- [x] 新建 `McpManager` (`src/llm/mcp/McpManager.h/.cpp`) 统一服务发现与生命周期调度

---

### [Milestone 5: 权限体系、断点恢复、DI 组合根与全量测试]

#### Phase 13：权限系统
- [x] 定义 `ToolPermission` 枚举 (ReadOnly, WriteWorkspace, ExecuteProcess, Network, ExternalService)
- [x] Tool 声明 permissions
- [x] AgentPolicy 评估权限 (Allow, AskUser, Deny)

#### Phase 14：Checkpoint / Resume
- [x] 定义 `AgentCheckpoint` 结构
- [x] 新建 `IAgentCheckpointRepository` (`src/domain/repository/IAgentCheckpointRepository.h`)
- [x] 新建 `SqliteAgentCheckpointRepository` (`src/data/repository/SqliteAgentCheckpointRepository.h/.cpp`) (使用 `SqlHelper`)
- [x] `AgentRuntime` 支持 `checkpoint()`
- [x] `AgentRuntime` 支持 `restore()`
- [x] `AgentRuntime` 支持 `suspend()`
- [x] `AgentRuntime` 支持 `resume()`

#### Phase 15：UDF 接入
- [x] 保持 Work 层单向数据流 UDF
- [x] 增加 `AgentRunUiState` 投影与状态流转
- [x] ToolEvent / Token / Thought 作为响应式状态投影

#### Phase 16：Composition Root
- [x] `ApplicationContext` 集中组装所有组件：
  - WorkspaceFileSystem
  - BuiltinToolProvider / McpToolProvider → ToolRegistry
  - SkillLoader → SkillRegistry
  - SqliteAgentRepository / SqliteAgentCheckpointRepository
  - AgentContextBuilder
  - AgentRuntime
  - RunAgentUseCase / CancelAgentRunUseCase / ResumeAgentRunUseCase
  - WorkViewModel

#### Phase 17：测试体系
- [x] `ToolRegistryTests`
- [x] `WorkspaceFileSystemTests`
- [x] `SkillLoaderTests`
- [x] `SkillRegistryTests`
- [x] `AgentContextBuilderTests`
- [x] `McpManagerConfigTests`
- [x] `SqliteAgentRepositoryTests`
- [x] `SqliteAgentCheckpointRepositoryTests`
- [x] `AgentPolicyPermissionTests`
- [x] `AgentProtocolTests` (4大模型协议适配器)
- [x] `WorkIntegrationTests`
