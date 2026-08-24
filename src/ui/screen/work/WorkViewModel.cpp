#include "WorkViewModel.h"

#include "application/usecase/chat/SendMessageUseCase.h"
#include "domain/service/IProjectContextService.h"
#include "services/agent/AgentToolService.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QUuid>
#include <algorithm>

namespace ui::screen::work {
namespace {
domain::conversation::Message& ensureStreamingMessage(WorkState& state) {
    if (!state.messages.isEmpty() && state.messages.last().role == domain::MessageRole::Assistant
        && state.messages.last().status == domain::MessageStatus::Sending) {
        return state.messages.last();
    }
    domain::conversation::Message message;
    message.id = QUuid::createUuid();
    message.role = domain::MessageRole::Assistant;
    message.status = domain::MessageStatus::Sending;
    message.createdAt = QDateTime::currentDateTime();
    state.messages.append(std::move(message));
    return state.messages.last();
}
}

WorkViewModel::WorkViewModel(const application::usecase::work::WorkUseCases& useCases,
                             domain::service::IProjectContextService* projectContext, QObject* parent)
    : BaseViewModel<WorkViewModel, WorkState>(parent), m_useCases(useCases), m_projectContext(projectContext),
      m_agentTools(useCases.agentTools), m_agentSessionId(QUuid::createUuid().toString(QUuid::WithoutBraces)) {
    setupUseCaseConnections();
    setProjectRoot(QDir::currentPath());
}

WorkViewModel::~WorkViewModel() = default;

void WorkViewModel::setupUseCaseConnections() {
    auto* agent = m_useCases.agentConversation;
    if (agent) {
        connect(agent, &application::usecase::chat::SendMessageUseCase::userMessageCreated, this,
                [this](const QString& sessionId, const domain::conversation::Message& message) {
            if (sessionId != m_agentSessionId) return;
            updateState([message](WorkState& state) {
                state.messages.append(message);
                state.isProcessing = true;
                state.statusMessage = QStringLiteral("项目 Agent 正在处理…");
            });
        });
        connect(agent, &application::usecase::chat::SendMessageUseCase::tokenReceived, this,
                [this](const QString& sessionId, const QString& token) {
            if (sessionId != m_agentSessionId || token.isEmpty()) return;
            updateState([token](WorkState& state) {
                auto& message = ensureStreamingMessage(state);
                for (auto& block : message.blocks) {
                    if (block.isText()) { std::get<domain::conversation::TextBlock>(block.payload).text += token; return; }
                }
                message.blocks.append({domain::BlockType::Text, domain::conversation::TextBlock{token}});
            });
        });
        connect(agent, &application::usecase::chat::SendMessageUseCase::thoughtReceived, this,
                [this](const QString& sessionId, const QString& thought) {
            if (sessionId != m_agentSessionId || thought.isEmpty()) return;
            updateState([thought](WorkState& state) {
                auto& message = ensureStreamingMessage(state);
                for (auto& block : message.blocks) {
                    if (block.isThought()) { std::get<domain::conversation::ThoughtBlock>(block.payload).thought += thought; return; }
                }
                message.blocks.append({domain::BlockType::Thought, domain::conversation::ThoughtBlock{thought, 0}});
            });
        });
        connect(agent, &application::usecase::chat::SendMessageUseCase::toolCallReceived, this,
                [this](const QString& sessionId, const domain::agent::ToolCall& call) {
            if (sessionId != m_agentSessionId) return;
            updateState([call](WorkState& state) {
                auto& message = ensureStreamingMessage(state);
                domain::conversation::ToolCallBlock calls; calls.calls.append(call);
                message.blocks.append({domain::BlockType::ToolCall, calls});
            });
        });
        connect(agent, &application::usecase::chat::SendMessageUseCase::toolResultReceived, this,
                [this](const QString& sessionId, const domain::agent::ToolResult& result) {
            if (sessionId != m_agentSessionId) return;
            updateState([result](WorkState& state) {
                auto& message = ensureStreamingMessage(state);
                domain::conversation::ToolResultBlock results; results.results.append(result);
                message.blocks.append({domain::BlockType::ToolResult, results});
            });
        });
        connect(agent, &application::usecase::chat::SendMessageUseCase::replyGenerated, this,
                [this](const QString& sessionId, const domain::conversation::Message& message) {
            if (sessionId != m_agentSessionId) return;
            updateState([message](WorkState& state) {
                if (!state.messages.isEmpty() && state.messages.last().role == domain::MessageRole::Assistant
                    && state.messages.last().status == domain::MessageStatus::Sending) state.messages.last() = message;
                else state.messages.append(message);
            });
        });
        connect(agent, &application::usecase::chat::SendMessageUseCase::generationFinished, this,
                [this](const QString& sessionId) {
            if (sessionId != m_agentSessionId) return;
            updateState([](WorkState& state) { state.isProcessing = false; state.statusMessage.clear(); });
        });
        connect(agent, &application::usecase::chat::SendMessageUseCase::generationFailed, this,
                [this](const QString& sessionId, const domain::llm::ChatError& error) {
            if (sessionId != m_agentSessionId) return;
            updateState([error](WorkState& state) {
                state.isProcessing = false;
                state.statusMessage = error.userMessage.isEmpty() ? error.message : error.userMessage;
            });
        });
        return;
    }

    // Dependency fallback for test/minimal composition roots.
    if (m_useCases.startTask) {
        connect(m_useCases.startTask, &application::usecase::work::StartTaskUseCase::taskStarted, this,
                [this](const QString& task) { updateState([task](WorkState& state) { state.currentTask = taskTitle(task); state.isProcessing = true; }); });
        connect(m_useCases.startTask, &application::usecase::work::StartTaskUseCase::toolFinished, this,
                [this](const domain::agent::ToolCall& call, const domain::agent::ToolResult& result) {
            updateState([&](WorkState& state) {
                domain::conversation::Message message;
                message.id = QUuid::createUuid(); message.role = domain::MessageRole::Assistant;
                message.createdAt = QDateTime::currentDateTime();
                domain::conversation::ToolCallBlock calls; calls.calls.append(call);
                domain::conversation::ToolResultBlock results; results.results.append(result);
                message.blocks.append({domain::BlockType::ToolCall, calls});
                message.blocks.append({domain::BlockType::ToolResult, results});
                state.messages.append(std::move(message));
            });
        });
        connect(m_useCases.startTask, &application::usecase::work::StartTaskUseCase::taskCompleted, this,
                [this](const QString&) { updateState([](WorkState& state) { state.isProcessing = false; }); });
    }
}

QString WorkViewModel::taskTitle(const QString& task) {
    const QString trimmed = task.trimmed();
    return trimmed.left(36) + (trimmed.size() > 36 ? QStringLiteral("…") : QString());
}

QString WorkViewModel::projectAgentPrompt() const {
    QString prompt = QStringLiteral("你是 ForgeAI 的项目 Agent。仅在项目工作区内工作，并在需要事实时优先使用提供的工具。"
                                    "不要臆造文件内容；执行工具后，基于工具结果继续回答。\n\n项目根目录：%1").arg(m_state.projectRoot);
    if (!m_state.agentInstructions.trimmed().isEmpty()) {
        prompt += QStringLiteral("\n\n以下是项目 AGENTS.md 和 Skills 指令，必须遵守：\n%1").arg(m_state.agentInstructions);
    }
    if (!m_state.mcpConfigContent.trimmed().isEmpty()) {
        prompt += QStringLiteral("\n\n以下是项目 MCP 配置上下文。可据此说明可用的集成；不要把未接入的 MCP 服务器当成已可调用工具：\n%1")
            .arg(m_state.mcpConfigContent.left(64 * 1024));
    }
    return prompt;
}

void WorkViewModel::startTask(const QString& task) {
    const QString trimmed = task.trimmed();
    if (trimmed.isEmpty()) return;
    updateState([trimmed](WorkState& state) { state.currentTask = taskTitle(trimmed); state.statusMessage.clear(); });
    if (m_useCases.agentConversation) {
        m_useCases.agentConversation->execute(m_agentSessionId, trimmed, {}, {}, false, false, {}, projectAgentPrompt());
    } else if (m_useCases.startTask) {
        m_useCases.startTask->execute(trimmed);
    }
}

void WorkViewModel::cancelTask() {
    if (m_useCases.agentConversation) m_useCases.agentConversation->cancelCurrent();
    else if (m_useCases.cancelTask) m_useCases.cancelTask->execute();
    updateState([](WorkState& state) { state.isProcessing = false; state.statusMessage.clear(); });
}

void WorkViewModel::setProjectRoot(const QString& rootPath) {
    if (!m_projectContext) return;
    const auto context = m_projectContext->load(rootPath);
    if (auto* tools = dynamic_cast<services::agent::AgentToolService*>(m_agentTools)) tools->setWorkspaceRoot(context.rootPath);
    updateState([&](WorkState& state) {
        state.projectRoot = context.rootPath;
        state.projectName = QFileInfo(context.rootPath).fileName();
        state.skillCount = context.skills.size();
        state.hasAgentsInstructions = !context.agentsInstructions.isEmpty();
        state.hasMcpConfig = !context.mcpConfigPath.isEmpty();
        state.mcpConfigContent = context.mcpConfigContent;
        QStringList instructions;
        if (!context.agentsInstructions.isEmpty()) instructions.append(context.agentsInstructions);
        for (const auto& skill : context.skills) instructions.append(QStringLiteral("# Skill: %1\n%2").arg(skill.name, skill.instructions));
        state.agentInstructions = instructions.join(QStringLiteral("\n\n"));
    });
}

void WorkViewModel::emitStateChanged() { Q_EMIT stateChanged(m_state); }
} // namespace ui::screen::work
