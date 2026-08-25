#include "AgentRuntime.h"
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
    ) : application::ports::IAgentRuntime(parent),
        m_chatGateway(chatGateway),
        m_conversationService(conversationService),
        m_toolRegistry(toolRegistry),
        m_checkpointRepo(checkpointRepo) {
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

        core::logging::LoggingService::instance().info(core::logging::Category::AgentRuntime, QStringLiteral("Agent 任务启动"), {
            {QStringLiteral("runId"), m_state.runId.toString(QUuid::WithoutBraces)},
            {QStringLiteral("sessionId"), context.sessionId},
            {QStringLiteral("projectId"), context.projectId.toString(QUuid::WithoutBraces)}
        });

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
        if (m_context.policy.timeoutMs > 0 && m_timeoutTimer) {
            m_timeoutTimer->start(m_context.policy.timeoutMs);
        }

        m_currentOp = m_chatGateway->sendRequest(m_context.provider, request);
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
        request.model = m_context.modelId.isEmpty() ? m_context.provider.id : m_context.modelId;
        request.stream = true;
        request.useWebSearch = m_context.useWebSearch;
        request.useDeepThinking = m_context.useDeepThinking;
        request.reasoningEffort = m_context.reasoningEffort;

        if (m_toolRegistry) {
            const auto allDefs = m_toolRegistry->definitions();
            QList<domain::agent::ToolDefinition> defs;
            if (!m_context.enabledTools.isEmpty()) {
                for (const auto& def : allDefs) {
                    if (m_context.enabledTools.contains(def.name)) {
                        defs.append(def);
                    }
                }
            } else {
                defs = allDefs;
            }
            if (!defs.isEmpty()) {
                request.tools = defs;
            }
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
        m_runCancellationToken.cancel();
        if (m_currentOp) {
            m_currentOp->cancel();
            cleanupCurrentOp();
        }
        m_replyBuffer.clear();
        m_thoughtBuffer.clear();
        m_activeToolCalls.clear();
        m_pendingToolResults.clear();
        m_pendingPermissions.clear();

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
                result = domain::agent::ToolResult{call.id, QStringLiteral("工具服务暂不可用，请稍后重试。"), true};
            }
            m_pendingToolResults.append(result);
            m_state.results = m_pendingToolResults;
            emit toolResultReady(m_context.sessionId, result);
            emit stateChanged(m_state);
        } else {
            core::logging::LoggingService::instance().info(core::logging::Category::AgentRuntime, QStringLiteral("用户拒绝工具权限授权"), {
                {QStringLiteral("toolName"), call.name},
                {QStringLiteral("callId"), call.id}
            });

            domain::agent::ToolResult result{
                call.id,
                QStringLiteral("你已拒绝该工具请求。"),
                true
            };
            m_pendingToolResults.append(result);
            m_state.results = m_pendingToolResults;
            emit toolResultReady(m_context.sessionId, result);
            emit stateChanged(m_state);
        }

        if (m_pendingPermissions.isEmpty()) {
            finishToolExecutionRound();
        }
    }

    void AgentRuntime::processExecutableToolCalls() {
        if (m_timeoutTimer) {
            m_timeoutTimer->stop();
        }

        application::ports::ToolExecutionContext execContext{
            m_context.workspaceRoot,
            m_context.sessionId,
            m_context.projectId,
            m_context.policy.timeoutMs > 0 ? m_context.policy.timeoutMs : 30000,
            m_runCancellationToken
        };

        bool hasPendingPermission = false;
        QList<domain::agent::ToolCall> parallelCalls;
        QList<domain::agent::ToolCall> serialCalls;

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

            // 1. 校验 enabledTools
            if (!m_context.enabledTools.isEmpty() && !m_context.enabledTools.contains(call.name)) {
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
                emit toolResultReady(m_context.sessionId, result);
                continue;
            }

            domain::agent::ToolPermission requiredPerm;
            domain::agent::PermissionDecision decision = domain::agent::PermissionDecision::Allow;
            bool isToolThreadSafe = true;

            if (m_toolRegistry) {
                auto tool = m_toolRegistry->findTool(call.name);
                if (tool) {
                    isToolThreadSafe = tool->isThreadSafe();
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
                emit toolResultReady(m_context.sessionId, result);
            } else if (decision == domain::agent::PermissionDecision::AskUser) {
                m_pendingPermissions[call.id] = {call, requiredPerm};
                hasPendingPermission = true;
                emit permissionRequested(m_context.sessionId, call, requiredPerm);
            } else {
                if (m_context.policy.allowParallelToolExecution && isToolThreadSafe) {
                    parallelCalls.append(call);
                } else {
                    serialCalls.append(call);
                }
            }
        }

        // 2. 统一受控并发执行所有线程安全的工具（涵盖 size >= 1 的所有情况）
        if (!parallelCalls.isEmpty()) {
            const int timeoutMs = execContext.timeoutMs;

            struct ParallelTask {
                domain::agent::ToolCall call;
                application::ports::CancellationToken token;
                std::shared_ptr<std::atomic<bool>> done = std::make_shared<std::atomic<bool>>(false);
                std::shared_ptr<domain::agent::ToolResult> result = std::make_shared<domain::agent::ToolResult>();
            };

            std::vector<ParallelTask> tasks;
            tasks.reserve(parallelCalls.size());

            auto cvMutex = std::make_shared<std::mutex>();
            auto cv = std::make_shared<std::condition_variable>();
            auto completedCount = std::make_shared<std::atomic<int>>(0);
            const int totalTasks = static_cast<int>(parallelCalls.size());

            for (const auto& call : parallelCalls) {
                ParallelTask task;
                task.call = call;
                task.token.linkParent(execContext.cancellationToken); // 使单次任务令牌可被全局取消
                application::ports::ToolExecutionContext taskContext = execContext;
                taskContext.cancellationToken = task.token; // 为此任务分配独立的取消令牌

                auto doneFlag = task.done;
                auto resultHolder = task.result;

                // 在主线程解析工具，剥离 this，防止超时后 AgentRuntime 析构导致的 UAF
                auto tool = m_toolRegistry ? m_toolRegistry->findTool(call.name) : nullptr;

                core::logging::LoggingService::instance().debug(core::logging::Category::AgentRuntime, QStringLiteral("派发并发工具调用"), {
                    {QStringLiteral("toolName"), call.name},
                    {QStringLiteral("callId"), call.id},
                    {QStringLiteral("argKeys"), core::logging::SensitiveDataFilter::extractArgKeys(call.arguments)}
                });

                std::thread([tool, call, taskContext, doneFlag, resultHolder, completedCount, cv, cvMutex]() {
                    domain::agent::ToolResult res;
                    if (tool) {
                        res = tool->execute(call, taskContext);
                    } else {
                        res = domain::agent::ToolResult{call.id, QStringLiteral("请求的工具当前不可用。"), true};
                    }
                    *resultHolder = std::move(res);
                    doneFlag->store(true, std::memory_order_release);
                    completedCount->fetch_add(1, std::memory_order_relaxed);
                    {
                        std::lock_guard<std::mutex> lock(*cvMutex);
                    }
                    cv->notify_one();
                }).detach();

                tasks.push_back(std::move(task));
            }

            // 限时等待任务完成（超时后立刻唤醒，不阻塞在工作线程析构上）
            {
                std::unique_lock<std::mutex> lock(*cvMutex);
                cv->wait_for(lock, std::chrono::milliseconds(timeoutMs), [&]() {
                    return completedCount->load(std::memory_order_relaxed) >= totalTasks;
                });
            }

            // 收集结果并对未完成的任务发出取消信号
            for (auto& task : tasks) {
                if (task.done->load(std::memory_order_acquire)) {
                    m_pendingToolResults.append(*task.result);
                    m_state.results = m_pendingToolResults;
                    emit toolResultReady(m_context.sessionId, *task.result);
                } else {
                    // 超时：发出合作式取消信号，立即生成超时结果并不阻塞流转
                    task.token.cancel();
                    core::logging::LoggingService::instance().warn(core::logging::Category::AgentRuntime, QStringLiteral("工具后台执行超时"), {
                        {QStringLiteral("toolName"), task.call.name},
                        {QStringLiteral("callId"), task.call.id},
                        {QStringLiteral("timeoutMs"), QString::number(timeoutMs)}
                    });

                    domain::agent::ToolResult timeoutResult{
                        task.call.id,
                        QStringLiteral("工具执行响应超时，请稍后重试。"),
                        true
                    };
                    m_pendingToolResults.append(timeoutResult);
                    m_state.results = m_pendingToolResults;
                    emit toolResultReady(m_context.sessionId, timeoutResult);
                }
            }
        }

        // 3. 在主线程顺序执行需保证 QObject 亲和性的工具（如 MCP / UI 交互）
        for (const auto& call : serialCalls) {
            core::logging::LoggingService::instance().debug(core::logging::Category::AgentRuntime, QStringLiteral("主线程执行工具调用"), {
                {QStringLiteral("toolName"), call.name},
                {QStringLiteral("callId"), call.id},
                {QStringLiteral("argKeys"), core::logging::SensitiveDataFilter::extractArgKeys(call.arguments)}
            });

            domain::agent::ToolResult result;
            if (m_toolRegistry) {
                result = m_toolRegistry->execute(call, execContext);
            } else {
                result = domain::agent::ToolResult{call.id, QStringLiteral("工具服务暂不可用，请稍后重试。"), true};
            }
            m_pendingToolResults.append(result);
            m_state.results = m_pendingToolResults;
            emit toolResultReady(m_context.sessionId, result);
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
                m_state.pendingCalls = m_activeToolCalls.values();
                emit toolCallStarted(m_context.sessionId, call);
                emit stateChanged(m_state);
            } else if constexpr (std::is_same_v<T, domain::llm::EventToolCallDelta>) {
                if (m_activeToolCalls.contains(arg.id)) {
                    m_activeToolCalls[arg.id].arguments += arg.argumentsDelta;
                    m_state.pendingCalls = m_activeToolCalls.values();
                }
            } else if constexpr (std::is_same_v<T, domain::llm::EventToolCallFinished>) {
                if (m_activeToolCalls.contains(arg.id)) {
                    const auto call = m_activeToolCalls[arg.id];
                    m_state.pendingCalls = m_activeToolCalls.values();
                    emit toolCallFinished(m_context.sessionId, call);
                    emit stateChanged(m_state);
                }
            } else if constexpr (std::is_same_v<T, domain::llm::EventFinished>) {
                if (m_timeoutTimer) {
                    m_timeoutTimer->stop();
                }
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
