#include "AgentRuntime.h"

#include <QDateTime>
#include <QUuid>
#include <QTimer>
#include "domain/conversation/MessageBlock.h"

namespace agent::runtime {

    AgentRuntime::AgentRuntime(
        application::ports::IChatModelGateway* chatGateway,
        domain::service::IConversationService* conversationService,
        agent::tool::ToolRegistry* toolRegistry,
        domain::repository::IAgentCheckpointRepository* checkpointRepo,
        QObject* parent
    ) : application::ports::IAgentRuntime(parent),
        m_chatGateway(chatGateway),
        m_conversationService(conversationService),
        m_toolRegistry(toolRegistry),
        m_checkpointRepo(checkpointRepo) {
    }

    AgentRuntime::~AgentRuntime() {
        cancelRun();
    }

    bool AgentRuntime::isRunning() const {
        return m_state.status == domain::agent::AgentRunStatus::Preparing
            || m_state.status == domain::agent::AgentRunStatus::CallingModel
            || m_state.status == domain::agent::AgentRunStatus::WaitingPermission
            || m_state.status == domain::agent::AgentRunStatus::WaitingTool
            || m_state.status == domain::agent::AgentRunStatus::ExecutingTool
            || m_state.status == domain::agent::AgentRunStatus::Continuing;
    }

    domain::agent::AgentRunState AgentRuntime::currentState() const {
        return m_state;
    }

    void AgentRuntime::setState(domain::agent::AgentRunStatus status, const QString& errorMessage) {
        m_state.status = status;
        m_state.errorMessage = errorMessage;
        emit stateChanged(m_state);
    }

    void AgentRuntime::cleanupCurrentOp() {
        if (m_currentOp) {
            m_currentOp->disconnect(this);
            m_currentOp->deleteLater();
            m_currentOp = nullptr;
        }
    }

    void AgentRuntime::saveCheckpoint() {
        if (!m_checkpointRepo || m_context.sessionId.isEmpty()) return;

        domain::agent::AgentCheckpoint cp;
        cp.checkpointId = QUuid::createUuid();
        cp.sessionId = m_context.sessionId;
        cp.runId = m_state.runId;
        cp.roundIndex = m_state.round;
        cp.status = m_state.status;
        cp.pendingToolCalls = m_activeToolCalls.values();
        cp.pendingToolResults = m_pendingToolResults;
        cp.timestamp = QDateTime::currentDateTime();

        m_checkpointRepo->saveCheckpoint(cp);
    }

    void AgentRuntime::startRun(const AgentRunContext& context, const QString& prompt) {
        const QString trimmed = prompt.trimmed();
        if (trimmed.isEmpty() || context.sessionId.isEmpty()) return;

        cancelRun();

        m_context = context;
        m_state.runId = context.runId.isNull() ? QUuid::createUuid() : context.runId;
        m_state.conversationId = context.sessionId;
        m_state.round = 0;
        m_state.pendingCalls.clear();
        m_state.results.clear();
        m_state.errorMessage.clear();
        m_pendingPermissions.clear();

        setState(domain::agent::AgentRunStatus::Preparing);

        // 1. 构建并持久化用户消息
        domain::conversation::Message userMsg;
        userMsg.id = QUuid::createUuid();
        userMsg.role = domain::MessageRole::User;
        userMsg.status = domain::MessageStatus::Sent;
        userMsg.createdAt = QDateTime::currentDateTime();
        userMsg.blocks.append(domain::conversation::MessageBlock(
            domain::BlockType::Text,
            domain::conversation::TextBlock{trimmed}
        ));

        if (m_conversationService) {
            auto history = m_conversationService->loadMessages(context.sessionId);
            history.append(userMsg);
            m_conversationService->saveMessages(context.sessionId, history);
        } else {
            m_transientHistories[context.sessionId].append(userMsg);
        }

        emit userMessageCreated(context.sessionId, userMsg);

        // 2. 发起第一轮模型请求
        startNextModelRequest();
    }

    void AgentRuntime::startNextModelRequest() {
        if (!m_chatGateway) {
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Configuration;
            err.code = QStringLiteral("MissingGateway");
            err.userMessage = QStringLiteral("网关服务未就绪。");
            setState(domain::agent::AgentRunStatus::Failed, err.userMessage);
            emit runFailed(m_context.sessionId, err);
            return;
        }

        setState(domain::agent::AgentRunStatus::CallingModel);

        m_replyBuffer.clear();
        m_thoughtBuffer.clear();
        m_activeToolCalls.clear();
        m_pendingToolResults.clear();
        m_pendingPermissions.clear();

        const auto history = m_conversationService
            ? m_conversationService->loadMessages(m_context.sessionId)
            : m_transientHistories.value(m_context.sessionId);

        const auto request = buildChatRequest(history);

        cleanupCurrentOp();
        m_currentOp = m_chatGateway->sendRequest(m_context.provider, request);
        if (!m_currentOp) {
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Provider;
            err.code = QStringLiteral("RequestStartFailed");
            err.userMessage = QStringLiteral("无法发起模型生成请求。");
            setState(domain::agent::AgentRunStatus::Failed, err.userMessage);
            emit runFailed(m_context.sessionId, err);
            return;
        }

        m_currentOp->setParent(this);
        connect(m_currentOp, &application::ports::IChatOperation::eventReceived,
                this, &AgentRuntime::onChatEventReceived);
    }

    domain::llm::ChatRequest AgentRuntime::buildChatRequest(
        const QList<domain::conversation::Message>& history
    ) const {
        domain::llm::ChatRequest request;
        request.model = m_context.modelId.isEmpty() ? m_context.provider.id : m_context.modelId;
        request.stream = true;
        request.useWebSearch = m_context.useWebSearch;
        request.useDeepThinking = m_context.useDeepThinking;
        request.reasoningEffort = m_context.reasoningEffort;

        if (m_toolRegistry) {
            request.tools = m_toolRegistry->definitions();
        }

        if (!m_context.systemPrompt.trimmed().isEmpty()) {
            request.messages.append({domain::MessageRole::System, m_context.systemPrompt});
        }

        for (const auto& msg : history) {
            if (msg.status != domain::MessageStatus::Sent) continue;

            domain::llm::ChatMessage llmMsg;
            llmMsg.role = msg.role;
            QList<domain::agent::ToolResult> results;
            QHash<QString, QString> toolNames;

            for (const auto& block : msg.blocks) {
                if (block.isText()) {
                    if (!llmMsg.content.isEmpty()) llmMsg.content += QLatin1Char('\n');
                    llmMsg.content += std::get<domain::conversation::TextBlock>(block.payload).text;
                } else if (block.isToolCall()) {
                    llmMsg.toolCalls = std::get<domain::conversation::ToolCallBlock>(block.payload).calls;
                    for (const auto& call : llmMsg.toolCalls.value()) {
                        toolNames.insert(call.id, call.name);
                    }
                } else if (block.isToolResult()) {
                    results.append(std::get<domain::conversation::ToolResultBlock>(block.payload).results);
                }
            }

            if (!llmMsg.content.isEmpty() || llmMsg.toolCalls.has_value()) {
                request.messages.append(llmMsg);
            }

            for (const auto& result : results) {
                domain::llm::ChatMessage toolMessage;
                toolMessage.role = domain::MessageRole::Tool;
                toolMessage.toolCallId = result.toolCallId;
                toolMessage.name = toolNames.value(result.toolCallId);
                toolMessage.content = result.content;
                request.messages.append(toolMessage);
            }
        }

        return request;
    }

    domain::conversation::Message AgentRuntime::makeAssistantMessage() const {
        domain::conversation::Message message;
        message.id = QUuid::createUuid();
        message.role = domain::MessageRole::Assistant;
        message.status = domain::MessageStatus::Sent;
        message.createdAt = QDateTime::currentDateTime();

        if (!m_thoughtBuffer.isEmpty()) {
            message.blocks.append({domain::BlockType::Thought, domain::conversation::ThoughtBlock{m_thoughtBuffer, 0}});
        }
        if (!m_replyBuffer.isEmpty()) {
            message.blocks.append({domain::BlockType::Text, domain::conversation::TextBlock{m_replyBuffer}});
        }
        if (!m_activeToolCalls.isEmpty()) {
            domain::conversation::ToolCallBlock calls;
            calls.calls = m_activeToolCalls.values();
            message.blocks.append({domain::BlockType::ToolCall, calls});
        }
        if (!m_pendingToolResults.isEmpty()) {
            domain::conversation::ToolResultBlock results;
            results.results = m_pendingToolResults;
            message.blocks.append({domain::BlockType::ToolResult, results});
        }
        return message;
    }

    void AgentRuntime::saveMessage(const domain::conversation::Message& message) {
        if (m_context.sessionId.isEmpty()) return;
        if (!m_conversationService) {
            m_transientHistories[m_context.sessionId].append(message);
            return;
        }
        auto history = m_conversationService->loadMessages(m_context.sessionId);
        history.append(message);
        m_conversationService->saveMessages(m_context.sessionId, history);
    }

    void AgentRuntime::cancelRun() {
        if (m_currentOp) {
            m_currentOp->cancel();
            cleanupCurrentOp();
        }
        m_replyBuffer.clear();
        m_thoughtBuffer.clear();
        m_activeToolCalls.clear();
        m_pendingToolResults.clear();
        m_pendingPermissions.clear();

        if (isRunning()) {
            setState(domain::agent::AgentRunStatus::Cancelled);
            saveCheckpoint();
            emit runCompleted(m_context.sessionId);
        }
    }

    void AgentRuntime::suspendRun() {
        if (m_currentOp) {
            m_currentOp->cancel();
            cleanupCurrentOp();
        }
        setState(domain::agent::AgentRunStatus::Suspended);
        saveCheckpoint();
    }

    void AgentRuntime::resumeRun(const AgentRunContext& context) {
        m_context = context;
        if (m_checkpointRepo) {
            auto cpOpt = m_checkpointRepo->getLatestCheckpoint(context.sessionId);
            if (cpOpt.has_value()) {
                m_state.round = cpOpt->roundIndex;
                m_state.runId = cpOpt->runId;
                m_activeToolCalls.clear();
                for (const auto& call : cpOpt->pendingToolCalls) {
                    m_activeToolCalls[call.id] = call;
                }
                m_pendingToolResults = cpOpt->pendingToolResults;

                if ((cpOpt->status == domain::agent::AgentRunStatus::WaitingPermission ||
                     cpOpt->status == domain::agent::AgentRunStatus::ExecutingTool) && !m_activeToolCalls.isEmpty()) {
                    processExecutableToolCalls();
                    return;
                }
            }
        }
        startNextModelRequest();
    }

    void AgentRuntime::grantPermission(const QString& sessionId, const QString& toolCallId, bool granted) {
        if (sessionId != m_context.sessionId) return;
        if (!m_pendingPermissions.contains(toolCallId)) return;

        const auto [call, perm] = m_pendingPermissions.take(toolCallId);

        if (granted) {
            application::ports::ToolExecutionContext execContext{
                m_context.workspaceRoot,
                m_context.sessionId,
                m_context.projectId
            };

            domain::agent::ToolResult result;
            if (m_toolRegistry) {
                result = m_toolRegistry->execute(call, execContext);
            } else {
                result = domain::agent::ToolResult{call.id, QStringLiteral("ToolRegistry 未就绪"), true};
            }
            m_pendingToolResults.append(result);
            emit toolResultReady(m_context.sessionId, result);
        } else {
            domain::agent::ToolResult result{
                call.id,
                QStringLiteral("用户拒绝授权执行该敏感操作: %1").arg(perm.reason),
                true
            };
            m_pendingToolResults.append(result);
            emit toolResultReady(m_context.sessionId, result);
        }

        if (m_pendingPermissions.isEmpty()) {
            finishToolExecutionRound();
        }
    }

    void AgentRuntime::processExecutableToolCalls() {
        application::ports::ToolExecutionContext execContext{
            m_context.workspaceRoot,
            m_context.sessionId,
            m_context.projectId
        };

        bool hasPendingPermission = false;

        for (const auto& call : m_activeToolCalls) {
            // 如果该工具调用已存在执行结果，跳过避免重复执行
            bool alreadyExecuted = false;
            for (const auto& existingRes : m_pendingToolResults) {
                if (existingRes.toolCallId == call.id) {
                    alreadyExecuted = true;
                    break;
                }
            }
            if (alreadyExecuted) continue;

            domain::agent::ToolPermission requiredPerm;
            domain::agent::PermissionDecision decision = domain::agent::PermissionDecision::Allow;

            if (m_toolRegistry) {
                auto tool = m_toolRegistry->findTool(call.name);
                if (tool) {
                    for (const auto& perm : tool->permissions()) {
                        auto d = m_context.policy.evaluatePermission(perm.type);
                        if (d == domain::agent::PermissionDecision::Deny) {
                            decision = domain::agent::PermissionDecision::Deny;
                            requiredPerm = perm;
                            break;
                        } else if (d == domain::agent::PermissionDecision::AskUser) {
                            decision = domain::agent::PermissionDecision::AskUser;
                            requiredPerm = perm;
                        }
                    }
                }
            }

            if (decision == domain::agent::PermissionDecision::Deny) {
                domain::agent::ToolResult result{
                    call.id,
                    QStringLiteral("安全策略拒绝执行操作: %1").arg(requiredPerm.reason),
                    true
                };
                m_pendingToolResults.append(result);
                emit toolResultReady(m_context.sessionId, result);
            } else if (decision == domain::agent::PermissionDecision::AskUser) {
                m_pendingPermissions[call.id] = {call, requiredPerm};
                hasPendingPermission = true;
                emit permissionRequested(m_context.sessionId, call, requiredPerm);
            } else {
                domain::agent::ToolResult result;
                if (m_toolRegistry) {
                    result = m_toolRegistry->execute(call, execContext);
                } else {
                    result = domain::agent::ToolResult{call.id, QStringLiteral("ToolRegistry 未就绪"), true};
                }
                m_pendingToolResults.append(result);
                emit toolResultReady(m_context.sessionId, result);
            }
        }

        if (hasPendingPermission) {
            setState(domain::agent::AgentRunStatus::WaitingPermission);
            saveCheckpoint();
        } else {
            finishToolExecutionRound();
        }
    }

    void AgentRuntime::finishToolExecutionRound() {
        setState(domain::agent::AgentRunStatus::ExecutingTool);

        // 保存包含 ToolResult 的消息记录
        if (!m_pendingToolResults.isEmpty()) {
            domain::conversation::Message resultMsg;
            resultMsg.id = QUuid::createUuid();
            resultMsg.role = domain::MessageRole::Tool;
            resultMsg.status = domain::MessageStatus::Sent;
            resultMsg.createdAt = QDateTime::currentDateTime();
            domain::conversation::ToolResultBlock rb;
            rb.results = m_pendingToolResults;
            resultMsg.blocks.append({domain::BlockType::ToolResult, rb});
            saveMessage(resultMsg);
        }

        saveCheckpoint();

        // 检查轮数上限
        if (m_state.round < m_context.policy.maxToolRounds) {
            ++m_state.round;
            setState(domain::agent::AgentRunStatus::Continuing);
            QTimer::singleShot(0, this, [this]() {
                startNextModelRequest();
            });
        } else {
            cleanupCurrentOp();
            setState(domain::agent::AgentRunStatus::Completed);
            saveCheckpoint();
            emit runCompleted(m_context.sessionId);
        }
    }

    void AgentRuntime::onChatEventReceived(const domain::llm::ChatEvent& event) {
        if (!isRunning() && m_state.status != domain::agent::AgentRunStatus::CallingModel) return;

        std::visit([this](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, domain::llm::EventStarted>) {
                // Started
            } else if constexpr (std::is_same_v<T, domain::llm::EventTextDelta>) {
                m_replyBuffer += arg.text;
                emit tokenReceived(m_context.sessionId, arg.text);
            } else if constexpr (std::is_same_v<T, domain::llm::EventThinkingDelta>) {
                m_thoughtBuffer += arg.thought;
                emit thoughtReceived(m_context.sessionId, arg.thought);
            } else if constexpr (std::is_same_v<T, domain::llm::EventToolCallStarted>) {
                domain::agent::ToolCall call{arg.id, arg.functionName, {}};
                m_activeToolCalls[arg.id] = call;
                emit toolCallStarted(m_context.sessionId, call);
            } else if constexpr (std::is_same_v<T, domain::llm::EventToolCallDelta>) {
                if (m_activeToolCalls.contains(arg.id)) {
                    m_activeToolCalls[arg.id].arguments += arg.argumentsDelta;
                }
            } else if constexpr (std::is_same_v<T, domain::llm::EventToolCallFinished>) {
                if (m_activeToolCalls.contains(arg.id)) {
                    const auto call = m_activeToolCalls[arg.id];
                    emit toolCallFinished(m_context.sessionId, call);
                }
            } else if constexpr (std::is_same_v<T, domain::llm::EventFinished>) {
                const auto assistantMsg = makeAssistantMessage();
                saveMessage(assistantMsg);
                emit replyGenerated(m_context.sessionId, assistantMsg);

                // 判断是否需要执行工具并 continuation
                if (!m_activeToolCalls.isEmpty()) {
                    processExecutableToolCalls();
                } else {
                    cleanupCurrentOp();
                    setState(domain::agent::AgentRunStatus::Completed);
                    saveCheckpoint();
                    emit runCompleted(m_context.sessionId);
                }
            } else if constexpr (std::is_same_v<T, domain::llm::EventError>) {
                const bool isCancelled = (arg.error.category == domain::llm::ChatErrorCategory::Cancelled);
                if (isCancelled) {
                    setState(domain::agent::AgentRunStatus::Cancelled);
                    saveCheckpoint();
                    emit runCompleted(m_context.sessionId);
                } else {
                    setState(domain::agent::AgentRunStatus::Failed, arg.error.userMessage.isEmpty() ? arg.error.message : arg.error.userMessage);
                    saveCheckpoint();
                    emit runFailed(m_context.sessionId, arg.error);
                }
                cleanupCurrentOp();
            }
        }, event);
    }

} // namespace agent::runtime
