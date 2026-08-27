#include "AgentRuntime.h"
#include "ToolExecutionScheduler.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"
#include "core/logging/SensitiveDataFilter.h"

#include <QDateTime>
#include <QUuid>
#include <QTimer>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "domain/conversation/MessageBlock.h"

namespace agent::runtime {

    AgentRuntime::AgentRuntime(
        application::ports::IChatModelGateway* chatGateway,
        domain::service::IConversationService* conversationService,
        agent::tool::ToolRegistry* toolRegistry,
        domain::repository::IAgentCheckpointRepository* checkpointRepo,
        QObject* parent
    ) : AgentRuntime(chatGateway, conversationService, toolRegistry, checkpointRepo, nullptr, parent) {
    }

    AgentRuntime::AgentRuntime(
        application::ports::IChatModelGateway* chatGateway,
        domain::service::IConversationService* conversationService,
        agent::tool::ToolRegistry* toolRegistry,
        domain::repository::IAgentCheckpointRepository* checkpointRepo,
        std::shared_ptr<application::ports::IProcessTaskRuntime> taskRuntime,
        QObject* parent
    ) : application::ports::IAgentRuntime(parent),
        m_chatGateway(chatGateway),
        m_conversationService(conversationService),
        m_toolRegistry(toolRegistry),
        m_checkpointRepo(checkpointRepo),
        m_taskRuntime(std::move(taskRuntime)) {
        m_timeoutTimer = new QTimer(this);
        m_timeoutTimer->setSingleShot(true);
        connect(m_timeoutTimer, &QTimer::timeout, this, &AgentRuntime::onTimeout);
    }

    AgentRuntime::~AgentRuntime() {
        m_runCancellationToken.cancel();
        if (m_currentOp) {
            m_currentOp->cancel();
            cleanupCurrentOp();
        }
        for (auto& op : m_activeOperations) {
            if (op) {
                disconnect(op.get(), &application::ports::IToolOperation::finished, this, nullptr);
                op->cancel();
            }
        }
        m_activeOperations.clear();
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
        m_state.pendingCalls = m_activeToolCalls.values();
        m_state.results = m_pendingToolResults;
        emit stateChanged(m_state);
    }

    void AgentRuntime::cleanupCurrentOp() {
        if (m_timeoutTimer) {
            m_timeoutTimer->stop();
        }
        if (m_currentOp) {
            m_currentOp->disconnect(this);
            m_currentOp->deleteLater();
            m_currentOp = nullptr;
        }
    }

    void AgentRuntime::onTimeout() {
        if (!isRunning()) return;
        cleanupCurrentOp();

        core::logging::LoggingService::instance().warn(core::logging::Category::AgentRuntime, QStringLiteral("Agent 操作执行超时"), {
            {QStringLiteral("runId"), m_state.runId.toString(QUuid::WithoutBraces)},
            {QStringLiteral("sessionId"), m_context.sessionId},
            {QStringLiteral("round"), QString::number(m_state.round)},
            {QStringLiteral("timeoutMs"), QString::number(m_context.policy.timeoutMs)}
        });

        domain::llm::ChatError err;
        err.category = domain::llm::ChatErrorCategory::Network;
        err.code = QStringLiteral("RequestTimeout");
        err.userMessage = QStringLiteral("任务响应超时，请重试。");
        setState(domain::agent::AgentRunStatus::Failed, err.userMessage);
        saveCheckpoint();
        emit runFailed(m_context.sessionId, err);
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
        m_runCancellationToken = application::ports::CancellationToken();

        m_context = context;
        m_state.runId = context.runId.isNull() ? QUuid::createUuid() : context.runId;
        m_state.conversationId = context.sessionId;
        m_state.round = 1;
        m_state.pendingCalls.clear();
        m_state.results.clear();
        m_state.errorMessage.clear();
        m_pendingPermissions.clear();
        m_runApprovedTools.clear();
        m_runApprovedCommands.clear();

        core::logging::LoggingService::instance().info(core::logging::Category::AgentRuntime, QStringLiteral("Agent 任务启动"), {
            {QStringLiteral("runId"), m_state.runId.toString(QUuid::WithoutBraces)},
            {QStringLiteral("sessionId"), context.sessionId},
            {QStringLiteral("projectId"), context.projectId.toString(QUuid::WithoutBraces)}
        });

        qInfo().noquote() << QStringLiteral("[AgentRuntime] === startRun ===\n  -> sessionId: %1\n  -> projectId: %2\n  -> provider: %3\n  -> model: %4\n  -> prompt: %5")
            .arg(context.sessionId, context.projectId.toString(QUuid::WithoutBraces), context.model.provider.id, context.model.requestModelId(), trimmed);

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
            qWarning().noquote() << QStringLiteral("[AgentRuntime] startNextModelRequest failed: MissingGateway");
            setState(domain::agent::AgentRunStatus::Failed, err.userMessage);
            emit runFailed(m_context.sessionId, err);
            return;
        }

        qInfo().noquote() << QStringLiteral("[AgentRuntime] startNextModelRequest -> Round: %1, Session: %2")
            .arg(QString::number(m_state.round), m_context.sessionId);

        setState(domain::agent::AgentRunStatus::CallingModel);

        m_replyBuffer.clear();
        m_thoughtBuffer.clear();
        m_toolCallOrder.clear();
        m_activeToolCalls.clear();
        m_pendingToolResults.clear();
        m_pendingPermissions.clear();

        m_currentAssistantMessageId = QUuid::createUuid();
        domain::conversation::Message assistantPlaceholder;
        assistantPlaceholder.id = m_currentAssistantMessageId;
        assistantPlaceholder.role = domain::MessageRole::Assistant;
        assistantPlaceholder.status = domain::MessageStatus::Sending;
        assistantPlaceholder.createdAt = QDateTime::currentDateTime();
        emit assistantMessageStarted(m_context.sessionId, assistantPlaceholder);

        const auto history = m_conversationService
            ? m_conversationService->loadMessages(m_context.sessionId)
            : m_transientHistories.value(m_context.sessionId);

        const auto request = buildChatRequest(history);

        cleanupCurrentOp();
        if (m_context.policy.timeoutMs > 0 && m_timeoutTimer) {
            m_timeoutTimer->start(m_context.policy.timeoutMs);
        }

        m_currentOp = m_chatGateway->sendRequest(m_context.model, request);
        if (!m_currentOp) {
            if (m_timeoutTimer) {
                m_timeoutTimer->stop();
            }
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
        request.model = m_context.model.requestModelId();
        request.stream = true;
        request.useWebSearch = m_context.useWebSearch;
        request.useDeepThinking = m_context.useDeepThinking;
        request.reasoningEffort = m_context.reasoningEffort;

        const bool supportsToolCalling = m_context.model.effectiveCapabilities().testFlag(domain::model::ModelCapability::ToolCalling);
        if (supportsToolCalling && m_toolRegistry && m_context.toolSelectionMode != ToolSelectionMode::None) {
            const auto allDefs = m_toolRegistry->definitions();
            QList<domain::agent::ToolDefinition> defs;
            if (m_context.toolSelectionMode == ToolSelectionMode::Selected) {
                for (const auto& def : allDefs) {
                    if (m_context.enabledTools.contains(def.name)) {
                        defs.append(def);
                    }
                }
            } else if (m_context.toolSelectionMode == ToolSelectionMode::All) {
                defs = allDefs;
            }
            if (!defs.isEmpty()) {
                request.tools = defs;
            }
        }

        if (!m_context.systemPrompt.trimmed().isEmpty()) {
            request.messages.append({domain::MessageRole::System, m_context.systemPrompt});
        }

        QHash<QString, QString> toolNames;
        for (const auto& msg : history) {
            if (msg.status != domain::MessageStatus::Sent) continue;

            domain::llm::ChatMessage llmMsg;
            llmMsg.role = msg.role;
            QList<domain::agent::ToolResult> results;

            for (const auto& block : msg.blocks) {
                if (block.isText()) {
                    if (!llmMsg.content.isEmpty()) llmMsg.content += QLatin1Char('\n');
                    llmMsg.content += std::get<domain::conversation::TextBlock>(block.payload).text;
                } else if (block.isThought()) {
                    if (!llmMsg.reasoningContent.isEmpty()) llmMsg.reasoningContent += QLatin1Char('\n');
                    llmMsg.reasoningContent += std::get<domain::conversation::ThoughtBlock>(block.payload).thought;
                } else if (block.isToolCall()) {
                    llmMsg.toolCalls = std::get<domain::conversation::ToolCallBlock>(block.payload).calls;
                    for (const auto& call : llmMsg.toolCalls.value()) {
                        toolNames.insert(call.id, call.name);
                    }
                } else if (block.isToolResult()) {
                    results.append(std::get<domain::conversation::ToolResultBlock>(block.payload).results);
                }
            }

            if (!llmMsg.content.isEmpty() || !llmMsg.reasoningContent.isEmpty() || llmMsg.toolCalls.has_value()) {
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

        const int toolsCount = request.tools.has_value() ? static_cast<int>(request.tools->size()) : 0;
        qInfo().noquote() << QStringLiteral("[AgentRuntime] buildChatRequest -> model: %1, tools: %2, messages: %3")
            .arg(request.model, QString::number(toolsCount), QString::number(request.messages.size()));

        return request;
    }

    domain::conversation::Message AgentRuntime::makeAssistantMessage() const {
        domain::conversation::Message message;
        message.id = m_currentAssistantMessageId.isNull() ? QUuid::createUuid() : m_currentAssistantMessageId;
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
            if (!m_toolCallOrder.isEmpty()) {
                for (const auto& id : m_toolCallOrder) {
                    if (m_activeToolCalls.contains(id)) {
                        calls.calls.append(m_activeToolCalls.value(id));
                    }
                }
            } else {
                calls.calls = m_activeToolCalls.values();
            }
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
        m_runCancellationToken.cancel();
        if (m_taskRuntime && !m_state.runId.isNull()) {
            m_taskRuntime->cancelTasksForRun(m_state.runId);
        }
        if (m_currentOp) {
            m_currentOp->cancel();
            cleanupCurrentOp();
        }
        // Drain ops one by one: disconnect finished first to prevent onToolOperationFinished
        // from modifying m_activeOperations mid-loop (iterator invalidation / recursive
        // executeNextBatch). Emit toolResultReady manually so callers receive a cancel result.
        while (!m_activeOperations.empty()) {
            auto op = std::move(m_activeOperations.front());
            m_activeOperations.erase(m_activeOperations.begin());
            if (!op) continue;
            disconnect(op.get(), &application::ports::IToolOperation::finished, this, nullptr);
            const domain::agent::ToolResult cancelResult{
                op->operationId(), QStringLiteral("操作已取消"), true
            };
            m_pendingToolResults.append(cancelResult);
            emit toolResultReady(m_context.sessionId, m_currentAssistantMessageId, cancelResult);
            op->cancel(); // stop timeout watchdog; background thread result will be discarded via QPointer
        } // op destroyed here — background threads see weakSelf==null and exit cleanly
        m_pendingBatches.clear();

        m_replyBuffer.clear();
        m_thoughtBuffer.clear();
        m_toolCallOrder.clear();
        m_activeToolCalls.clear();
        m_pendingToolResults.clear();
        m_pendingPermissions.clear();
        m_runApprovedTools.clear();

        core::logging::LoggingService::instance().info(core::logging::Category::AgentRuntime, QStringLiteral("Agent 任务已取消"), {
            {QStringLiteral("runId"), m_state.runId.toString(QUuid::WithoutBraces)},
            {QStringLiteral("sessionId"), m_context.sessionId}
        });

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
                m_toolCallOrder.clear();
                m_activeToolCalls.clear();
                for (const auto& call : cpOpt->pendingToolCalls) {
                    m_toolCallOrder.append(call.id);
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

    void AgentRuntime::grantPermission(
        const QString& sessionId,
        const QString& toolCallId,
        bool granted,
        domain::agent::PermissionScope scope,
        const QString& customInput
    ) {
        if (sessionId != m_context.sessionId) return;
        if (!m_pendingPermissions.contains(toolCallId)) return;

        const auto [call, perm] = m_pendingPermissions.take(toolCallId);

        if (granted) {
            if (call.name == QStringLiteral("run_command")) {
                const QJsonObject args = QJsonDocument::fromJson(call.arguments.toUtf8()).object();
                const QString command = args.value(QStringLiteral("command")).toString().trimmed();
                if (!command.isEmpty()) {
                    if (scope == domain::agent::PermissionScope::Run) {
                        m_runApprovedCommands.insert(command);
                        core::logging::LoggingService::instance().info(core::logging::Category::AgentRuntime, QStringLiteral("已在当前对话记住命令授权"), {
                            {QStringLiteral("command"), command}
                        });
                    } else if (scope == domain::agent::PermissionScope::Project) {
                        m_projectApprovedCommands[m_context.projectId].insert(command);
                        core::logging::LoggingService::instance().info(core::logging::Category::AgentRuntime, QStringLiteral("已在当前项目记住命令授权"), {
                            {QStringLiteral("command"), command},
                            {QStringLiteral("projectId"), m_context.projectId.toString()}
                        });
                    } else if (scope == domain::agent::PermissionScope::Global) {
                        m_globalApprovedCommands.insert(command);
                        core::logging::LoggingService::instance().info(core::logging::Category::AgentRuntime, QStringLiteral("已全局永久记住命令授权"), {
                            {QStringLiteral("command"), command}
                        });
                    }
                }
            } else {
                if (scope == domain::agent::PermissionScope::Run) {
                    m_runApprovedTools.insert(call.name);
                    core::logging::LoggingService::instance().info(core::logging::Category::AgentRuntime, QStringLiteral("用户已记住当前会话内的工具授权"), {
                        {QStringLiteral("toolName"), call.name}
                    });
                } else if (scope == domain::agent::PermissionScope::Project) {
                    m_projectApprovedTools[m_context.projectId].insert(call.name);
                    core::logging::LoggingService::instance().info(core::logging::Category::AgentRuntime, QStringLiteral("用户已记住当前项目内的工具授权"), {
                        {QStringLiteral("toolName"), call.name},
                        {QStringLiteral("projectId"), m_context.projectId.toString()}
                    });
                } else if (scope == domain::agent::PermissionScope::Global) {
                    m_globalApprovedTools.insert(call.name);
                    core::logging::LoggingService::instance().info(core::logging::Category::AgentRuntime, QStringLiteral("用户已记住全局工具授权"), {
                        {QStringLiteral("toolName"), call.name}
                    });
                }
            }

            const int toolTimeout = m_context.policy.toolTimeoutMs > 0 ? m_context.policy.toolTimeoutMs : (m_context.policy.timeoutMs > 0 ? m_context.policy.timeoutMs : 30000);
            application::ports::ToolExecutionContext execContext{
                m_state.runId,
                m_context.sessionId,
                m_context.projectId,
                m_context.workspaceRoot,
                toolTimeout,
                m_runCancellationToken,
                QString()
            };

            auto tool = m_toolRegistry->findTool(call.name);
            auto op = tool->execute(call, execContext);
            auto* opPtr = op.get();
            connect(opPtr, &application::ports::IToolOperation::finished, this, [this, toolCallId = call.id](const domain::agent::ToolResult& result) {
                onToolOperationFinished(toolCallId, result);
            });
            m_activeOperations.push_back(std::move(op));
            opPtr->start();
        } else {
            core::logging::LoggingService::instance().info(core::logging::Category::AgentRuntime, QStringLiteral("用户跳过/拒绝工具权限授权"), {
                {QStringLiteral("toolName"), call.name},
                {QStringLiteral("callId"), call.id},
                {QStringLiteral("customInput"), customInput}
            });

            const QString rejectMessage = customInput.trimmed().isEmpty()
                ? QStringLiteral("Command was skipped by user.")
                : QStringLiteral("Command was skipped by user. Feedback: %1").arg(customInput.trimmed());

            domain::agent::ToolResult result{
                call.id,
                rejectMessage,
                true,
                QStringLiteral("Skipped")
            };
            m_pendingToolResults.append(result);
            m_state.results = m_pendingToolResults;
            emit toolResultReady(m_context.sessionId, m_currentAssistantMessageId, result);
            emit stateChanged(m_state);
            saveCheckpoint();

            if (m_pendingPermissions.isEmpty() && m_activeOperations.empty() && m_pendingBatches.isEmpty()) {
                finishToolExecutionRound();
            }
        }
    }

    void AgentRuntime::processExecutableToolCalls() {
        if (m_timeoutTimer) {
            m_timeoutTimer->stop();
        }

        bool hasPendingPermission = false;
        QList<domain::agent::ToolCall> executableCalls;

        QList<domain::agent::ToolCall> callsToProcess;
        for (const auto& id : m_toolCallOrder) {
            if (m_activeToolCalls.contains(id)) {
                callsToProcess.append(m_activeToolCalls.value(id));
            }
        }
        if (callsToProcess.isEmpty()) {
            callsToProcess = m_activeToolCalls.values();
        }

        for (const auto& call : callsToProcess) {
            bool alreadyExecuted = false;
            for (const auto& existingRes : m_pendingToolResults) {
                if (existingRes.toolCallId == call.id) {
                    alreadyExecuted = true;
                    break;
                }
            }
            if (alreadyExecuted) continue;

            const bool toolDisallowed = (m_context.toolSelectionMode == ToolSelectionMode::None)
                || (m_context.toolSelectionMode == ToolSelectionMode::Selected && !m_context.enabledTools.contains(call.name));
            if (toolDisallowed) {
                core::logging::LoggingService::instance().warn(core::logging::Category::AgentRuntime, QStringLiteral("工具未在启用列表中被拦截"), {
                    {QStringLiteral("toolName"), call.name},
                    {QStringLiteral("callId"), call.id}
                });

                domain::agent::ToolResult result{
                    call.id,
                    QStringLiteral("安全策略拒绝执行操作：工具 '%1' 未在当前智能体启用列表中。").arg(call.name),
                    true
                };
                m_pendingToolResults.append(result);
                m_state.results = m_pendingToolResults;
                emit toolResultReady(m_context.sessionId, m_currentAssistantMessageId, result);
                saveCheckpoint();
                continue;
            }

            domain::agent::ToolPermission requiredPerm;
            domain::agent::PermissionDecision decision = domain::agent::PermissionDecision::Allow;

            // 如果是 run_command，检查命令级白名单 (会话 / 项目 / 全局)
            bool isCommandApproved = false;
            if (call.name == QStringLiteral("run_command")) {
                const QJsonObject args = QJsonDocument::fromJson(call.arguments.toUtf8()).object();
                const QString command = args.value(QStringLiteral("command")).toString().trimmed();
                if (!command.isEmpty()) {
                    if (m_runApprovedCommands.contains(command) ||
                        m_globalApprovedCommands.contains(command) ||
                        m_projectApprovedCommands.value(m_context.projectId).contains(command)) {
                        isCommandApproved = true;
                    }
                }
            }

            // 如果当前命令/工具已在会话、项目或全局授权，直接允许
            if (isCommandApproved ||
                m_runApprovedTools.contains(call.name) ||
                m_globalApprovedTools.contains(call.name) ||
                m_projectApprovedTools.value(m_context.projectId).contains(call.name)) {
                decision = domain::agent::PermissionDecision::Allow;
            } else if (m_toolRegistry) {
                auto tool = m_toolRegistry->findTool(call.name);
                if (tool) {
                    for (const auto& perm : tool->permissions(call)) {
                        auto d = m_context.policy.evaluateTool(call.name, perm);
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
                core::logging::LoggingService::instance().warn(core::logging::Category::AgentRuntime, QStringLiteral("安全策略拒绝工具执行"), {
                    {QStringLiteral("toolName"), call.name},
                    {QStringLiteral("callId"), call.id},
                    {QStringLiteral("reason"), requiredPerm.reason}
                });

                domain::agent::ToolResult result{
                    call.id,
                    QStringLiteral("安全策略拒绝执行操作: %1").arg(requiredPerm.reason),
                    true
                };
                m_pendingToolResults.append(result);
                m_state.results = m_pendingToolResults;
                emit toolResultReady(m_context.sessionId, m_currentAssistantMessageId, result);
                saveCheckpoint();
            } else if (decision == domain::agent::PermissionDecision::AskUser) {
                m_pendingPermissions[call.id] = {call, requiredPerm};
                hasPendingPermission = true;
                emit permissionRequested(m_context.sessionId, call, requiredPerm);
            } else {
                executableCalls.append(call);
            }
        }

        if (hasPendingPermission) {
            qInfo().noquote() << QStringLiteral("[AgentRuntime] processExecutableToolCalls -> Waiting user permission");
            setState(domain::agent::AgentRunStatus::WaitingPermission);
            saveCheckpoint();
            return;
        }

        if (executableCalls.isEmpty()) {
            qInfo().noquote() << QStringLiteral("[AgentRuntime] processExecutableToolCalls -> No executable calls, finishing round");
            finishToolExecutionRound();
            return;
        }

        qInfo().noquote() << QStringLiteral("[AgentRuntime] processExecutableToolCalls -> Executing %1 tool calls").arg(QString::number(executableCalls.size()));
        for (const auto& call : executableCalls) {
            qInfo().noquote() << QStringLiteral("  -> Tool [%1] id=%2 args=%3").arg(call.name, call.id, call.arguments);
        }

        setState(domain::agent::AgentRunStatus::ExecutingTool);
        saveCheckpoint();

        m_pendingBatches = ToolExecutionScheduler::scheduleBatches(
            executableCalls,
            m_toolRegistry,
            m_context.policy.allowParallelToolExecution
        );

        executeNextBatch();
    }

    void AgentRuntime::executeNextBatch() {
        if (m_pendingBatches.isEmpty()) {
            if (m_pendingPermissions.isEmpty() && m_activeOperations.empty()) {
                finishToolExecutionRound();
            }
            return;
        }

        auto currentBatch = m_pendingBatches.takeFirst();
        if (currentBatch.isEmpty()) {
            executeNextBatch();
            return;
        }

        const int toolTimeout = m_context.policy.toolTimeoutMs > 0 ? m_context.policy.toolTimeoutMs : (m_context.policy.timeoutMs > 0 ? m_context.policy.timeoutMs : 30000);
        application::ports::ToolExecutionContext execContext{
            m_state.runId,
            m_context.sessionId,
            m_context.projectId,
            m_context.workspaceRoot,
            toolTimeout,
            m_runCancellationToken,
            QString()
        };

        for (const auto& call : currentBatch) {
            if (m_runCancellationToken.isCanceled()) return;

            auto callContext = execContext;
            callContext.executionId = call.id;

            core::logging::LoggingService::instance().debug(core::logging::Category::AgentRuntime, QStringLiteral("派发异步工具调用"), {
                {QStringLiteral("toolName"), call.name},
                {QStringLiteral("callId"), call.id},
                {QStringLiteral("argKeys"), core::logging::SensitiveDataFilter::extractArgKeys(call.arguments)}
            });

            qInfo().noquote() << QStringLiteral("[AgentRuntime] executeToolOperation -> %1 (id: %2)").arg(call.name, call.id);

            std::unique_ptr<application::ports::IToolOperation> op;
            if (m_toolRegistry) {
                op = m_toolRegistry->execute(call, callContext);
            } else {
                op = std::make_unique<application::ports::ImmediateToolOperation>(
                    call.id,
                    [call]() {
                        return domain::agent::ToolResult{call.id, QStringLiteral("工具服务暂不可用，请稍后重试。"), true};
                    }
                );
            }

            auto* opPtr = op.get();
            const QString toolCallId = call.id;
            connect(opPtr, &application::ports::IToolOperation::finished, this, [this, toolCallId](const domain::agent::ToolResult& result) {
                onToolOperationFinished(toolCallId, result);
            });

            m_activeOperations.push_back(std::move(op));
            opPtr->start();
        }
    }

    void AgentRuntime::onToolOperationFinished(const QString& toolCallId, const domain::agent::ToolResult& result) {
        domain::agent::ToolResult safeResult = result;
        const int maxOutputChars = m_context.policy.maxToolOutputChars > 0 ? m_context.policy.maxToolOutputChars : 32768;
        if (safeResult.content.length() > maxOutputChars) {
            const int originalLen = safeResult.content.length();
            safeResult.content = safeResult.content.left(maxOutputChars) +
                QStringLiteral("\n\n[工具输出已截断：原始内容共 %1 字符，仅保留前 %2 字符以保护上下文窗口]").arg(originalLen).arg(maxOutputChars);
        }

        qInfo().noquote() << QStringLiteral("[AgentRuntime] onToolOperationFinished -> id=%1, isError=%2, outputLen=%3, snippet: %4")
            .arg(toolCallId, safeResult.isError ? QStringLiteral("true") : QStringLiteral("false"),
                 QString::number(safeResult.content.length()), safeResult.content.left(80).replace('\n', ' '));

        m_pendingToolResults.append(safeResult);
        m_state.results = m_pendingToolResults;
        emit toolResultReady(m_context.sessionId, m_currentAssistantMessageId, safeResult);
        emit stateChanged(m_state);
        saveCheckpoint();

        for (auto it = m_activeOperations.begin(); it != m_activeOperations.end(); ++it) {
            if ((*it) && (*it)->operationId() == toolCallId) {
                m_activeOperations.erase(it);
                break;
            }
        }

        if (m_activeOperations.empty()) {
            executeNextBatch();
        }
    }

    void AgentRuntime::finishToolExecutionRound() {
        setState(domain::agent::AgentRunStatus::ExecutingTool);

        // 结果保序重排
        QList<domain::agent::ToolResult> orderedResults;
        for (const auto& callId : m_toolCallOrder) {
            for (const auto& res : m_pendingToolResults) {
                if (res.toolCallId == callId) {
                    orderedResults.append(res);
                    break;
                }
            }
        }
        for (const auto& res : m_pendingToolResults) {
            bool exists = false;
            for (const auto& ord : orderedResults) {
                if (ord.toolCallId == res.toolCallId) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                orderedResults.append(res);
            }
        }
        m_pendingToolResults = orderedResults;
        m_state.results = m_pendingToolResults;

        setState(domain::agent::AgentRunStatus::PersistingToolResult);

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
            qInfo().noquote() << QStringLiteral("[AgentRuntime] Continuing to Round %1 (max %2)...")
                .arg(QString::number(m_state.round), QString::number(m_context.policy.maxToolRounds));
            setState(domain::agent::AgentRunStatus::Continuing);
            QTimer::singleShot(0, this, [this]() {
                startNextModelRequest();
            });
        } else {
            qInfo().noquote() << QStringLiteral("[AgentRuntime] Reached maxToolRounds (%1), completing run.")
                .arg(QString::number(m_context.policy.maxToolRounds));
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
                qInfo().noquote() << QStringLiteral("[AgentRuntime] EventStarted received");
            } else if constexpr (std::is_same_v<T, domain::llm::EventTextDelta>) {
                m_replyBuffer += arg.text;
                emit tokenReceived(m_context.sessionId, m_currentAssistantMessageId, arg.text);
            } else if constexpr (std::is_same_v<T, domain::llm::EventThinkingDelta>) {
                m_thoughtBuffer += arg.thought;
                emit thoughtReceived(m_context.sessionId, m_currentAssistantMessageId, arg.thought);
            } else if constexpr (std::is_same_v<T, domain::llm::EventToolCallStarted>) {
                qInfo().noquote() << QStringLiteral("[AgentRuntime] EventToolCallStarted -> tool: %1, id: %2").arg(arg.functionName, arg.id);
                domain::agent::ToolCall call{arg.id, arg.functionName, {}};
                call.protocolMetadata = arg.protocolMetadata; // 必须完整保留 thoughtSignature
                if (!m_toolCallOrder.contains(arg.id)) {
                    m_toolCallOrder.append(arg.id);
                }
                m_activeToolCalls[arg.id] = call;

                QList<domain::agent::ToolCall> orderedCalls;
                for (const auto& callId : m_toolCallOrder) {
                    if (m_activeToolCalls.contains(callId)) orderedCalls.append(m_activeToolCalls.value(callId));
                }
                m_state.pendingCalls = orderedCalls;
                emit stateChanged(m_state);
            } else if constexpr (std::is_same_v<T, domain::llm::EventToolCallDelta>) {
                if (m_activeToolCalls.contains(arg.id)) {
                    m_activeToolCalls[arg.id].arguments += arg.argumentsDelta;
                    QList<domain::agent::ToolCall> orderedCalls;
                    for (const auto& callId : m_toolCallOrder) {
                        if (m_activeToolCalls.contains(callId)) orderedCalls.append(m_activeToolCalls.value(callId));
                    }
                    m_state.pendingCalls = orderedCalls;
                } else {
                    qWarning().noquote() << QStringLiteral("[AgentRuntime] EventToolCallDelta for unknown id: %1").arg(arg.id);
                }
            } else if constexpr (std::is_same_v<T, domain::llm::EventToolCallFinished>) {
                if (m_activeToolCalls.contains(arg.id)) {
                    const auto call = m_activeToolCalls[arg.id];
                    qInfo().noquote() << QStringLiteral("[AgentRuntime] EventToolCallFinished -> id: %1, totalArgs: %2").arg(arg.id, call.arguments);
                    QList<domain::agent::ToolCall> orderedCalls;
                    for (const auto& callId : m_toolCallOrder) {
                        if (m_activeToolCalls.contains(callId)) orderedCalls.append(m_activeToolCalls.value(callId));
                    }
                    m_state.pendingCalls = orderedCalls;
                    emit toolCallFinished(m_context.sessionId, m_currentAssistantMessageId, call);
                    emit stateChanged(m_state);
                }
            } else if constexpr (std::is_same_v<T, domain::llm::EventFinished>) {
                if (m_timeoutTimer) {
                    m_timeoutTimer->stop();
                }
                qInfo().noquote() << QStringLiteral("[AgentRuntime] EventFinished -> reason: %1, replyLen: %2, thoughtLen: %3, toolCalls: %4")
                    .arg(arg.finishReason, QString::number(m_replyBuffer.length()),
                         QString::number(m_thoughtBuffer.length()), QString::number(m_activeToolCalls.size()));
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
                if (m_timeoutTimer) {
                    m_timeoutTimer->stop();
                }
                qWarning().noquote() << QStringLiteral("[AgentRuntime] EventError -> code: %1, msg: %2, userMsg: %3")
                    .arg(arg.error.code, arg.error.message, arg.error.userMessage);
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
