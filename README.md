<div align="center">
  <img src="res/icons/forgeai.png" alt="ForgeAI Logo" width="120" />
  <h1>ForgeAI</h1>
  <p>
    <b>一个基于 C++23 与 Qt 6 构建的高性能桌面级多模型 AI 智能工作台与 Agent 宿主环境。</b>
  </p>

<!-- 占位 Badges -->
[![C++23](https://img.shields.io/badge/C++-23-blue.svg?style=flat-square&logo=c%2B%2B)](https://isocpp.org/)
[![Qt6](https://img.shields.io/badge/Qt-6.x-41CD52.svg?style=flat-square&logo=qt)](https://www.qt.io/)
[![CMake](https://img.shields.io/badge/CMake-3.21+-064F8C.svg?style=flat-square&logo=cmake)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)

</div>

## ✨ 核心特性

- 🤖 **统一多模型协议网关**
  无缝接入并统一切换各大主流模型服务提供商。原生支持 OpenAI Chat Completions、OpenAI Responses、Anthropic、Google Gemini 以及基于 Ollama 的本地离线大模型部署，支持 SSE 丝滑流式响应。
- 🛠️ **全能型 Agent 智能体与沙箱工具**
  依托大模型原生 Function Calling 能力，内置文件读写、目录扫描、终端命令执行等工具沙箱。同时引入严格的 **HITL（Human-In-The-Loop 人在回路）权限审批机制**，赋予 AI 探索本地工作区、自动化处理任务的能力且兼顾安全隔离。
- 🔌 **深度集成 MCP (Model Context Protocol)**
  全面兼容 Anthropic 提出的 MCP 标准协议。支持通过 `stdio` 进程或 `http` 远程端点无缝接入外部能力（如跨语言 LSP、远程数据库访问等），为 Agent 注入无限的上下文感知与外部工具执行潜能。
- ⚡ **极致性能的富文本渲染**
  告别 WebEngine 与沉重的传统渲染引擎。ForgeAI 自研基于 `cmark-gfm` + `QPainter` 的**原生高性能 Markdown 引擎**，结合**虚拟化可视域卡片池（Virtual Scrolling）**，在承载上万条包含海量代码块的长篇对话时仍能保持 60FPS 顺滑体验。
- 🎨 **现代化桌面 UI**
  深度融合 Fluent Design 视觉风格，搭配亚克力模糊与自适应明暗主题切换，支持全局快捷键与跨平台无边框沉浸式窗口，打造极致清爽的开发者交互体验。

---

## 🏗️ 软件架构

项目严格遵循 **Clean Architecture (整洁架构)** 思想，按职责将系统划分为高内聚低耦合的层次模块，所有的依赖均在主线程入口进行 **DI（依赖注入）容器组装**，极大地增强了系统的可测试性与可扩展性。

- **`src/domain/`**：领域层，提炼纯粹的实体、值对象与核心协议接口。
- **`src/application/`**：应用层，组装核心业务 UseCase，定义出入端口 Port。
- **`src/agent/` & `src/llm/`**：运行时层，负责多轮 Agent 工具循环调度、上下文组装引擎与多模型传输适配。
- **`src/data/`**：基础设施层，采用 SQLite 持久化结构化元数据，JSONL 日志引擎承接流式对话留痕。
- **`src/ui/`**：表现层，基于 MVVM 与单向数据流 (UDF)，负责界面的数据绑定与事件分发。

---

## 🚀 快速开始

### 依赖环境

- 编译标准：**C++23**
- 编译系统：**CMake (>= 3.21)**
- 核心框架：**Qt (>= 6.x)** (需包含 `Core`, `Gui`, `Widgets`, `Network`, `Sql`, `Test` 模块)
- 第三方依赖（CMake 自动拉取）：`Fluent-Qt`, `qwindowkit`, `cmark-gfm`

### 构建步骤

```bash
# 1. 克隆代码库
git clone https://github.com/DengYJie/ForgeAI.git
cd ForgeAI

# 2. 生成构建文件
cmake -B build -S . -G "Ninja" # 或选用 Visual Studio 17 2022 等生成器

# 3. 编译工程
cmake --build build --config Release -j12

# 4. 运行
./build/Release/ForgeAI.exe
```

---

## 📚 开发与测试

为了保证核心逻辑的稳健性，项目中内置了基于 `QtTest` 的自动化测试集。
主要涵盖 Markdown 渲染准确性、Agent 任务沙箱隔离、数据持久化等方面。

```bash
# 开启 CMake 测试支持并编译测试目标
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build --target MarkdownCoreTests AgentToolTests AgentProtocolTests -j12

# 执行测试用例
ctest --test-dir build --output-on-failure
```


