#include "SendMessageUseCase.h"
#include "domain/service/IConversationService.h"
#include "domain/service/IModelService.h"
#include "domain/llm/ChatRequest.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"
#include <QDateTime>
#include <QUuid>
#include <algorithm>

namespace application::usecase::chat {

    SendMessageUseCase::SendMessageUseCase(
        ports::IChatModelGateway *chatGateway,
        domain::service::IConversationService *conversationService,
        domain::service::IModelService *modelService,
        QObject *parent
    ) : QObject(parent),
        m_chatGateway(chatGateway),
        m_conversationService(conversationService),
        m_modelService(modelService) {
    }

    SendMessageUseCase::~SendMessageUseCase() {
        cancelCurrent();
    }

    void SendMessageUseCase::execute(const QString &sessionId, const QString &text,
                                     const QString &providerId, const QString &modelId,
                                     bool useWebSearch, bool useDeepThinking, const QString& reasoningEffort,
                                     const QString& systemPrompt) {
        const QString trimmed = text.trimmed();
        if (trimmed.isEmpty() || sessionId.isEmpty()) return;
        
        // 1. 获取模型配置
        if (!m_modelService || !m_chatGateway) {
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Configuration;
            err.code = QStringLiteral("MissingDependencies");
            err.message = QStringLiteral("System dependencies are not ready.");
            err.userMessage = QStringLiteral("系统服务依赖未就绪。");
            emit generationFailed(sessionId, err);
            return;
        }
        
        const auto models = m_modelService->getEnabledResolvedModels();
        if (models.isEmpty()) {
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Configuration;
            err.code = QStringLiteral("NoActiveProvider");
            err.message = QStringLiteral("No model provider configured.");
            err.userMessage = QStringLiteral("未配置或启用任何模型提供商，请在设置中添加。");
            err.suggestedAction = QStringLiteral("OpenSettings");
            emit generationFailed(sessionId, err);
            return;
        }
        const auto selected = std::find_if(models.cbegin(), models.cend(), [&](const domain::model::ResolvedModel& model) {
            return (providerId.isEmpty() || model.provider.id == providerId)
                && (modelId.isEmpty() || model.requestModelId() == modelId);
        });
        if (selected == models.cend()) {
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Configuration;
            err.code = QStringLiteral("SelectedModelUnavailable");
            err.message = QStringLiteral("The selected model is no longer enabled.");
            err.userMessage = QStringLiteral("所选模型不可用，请重新选择。");
            emit generationFailed(sessionId, err);
            return;
        }
        const domain::model::ModelProvider provider = selected->provider;
        
        // 2. 创建并保存用户消息
        domain::conversation::Message userMsg;
        userMsg.id = QUuid::createUuid();
        userMsg.role = domain::MessageRole::User;
        userMsg.status = domain::MessageStatus::Sent;
        userMsg.createdAt = QDateTime::currentDateTime();
        userMsg.blocks.append(domain::conversation::MessageBlock(
            domain::BlockType::Text,
            domain::conversation::TextBlock{trimmed}
        ));

        QList<domain::conversation::Message> history;
        if (m_conversationService) {
            history = m_conversationService->loadMessages(sessionId);
            history.append(userMsg);
            m_conversationService->saveMessages(sessionId, history);
        } else {
            auto& transientHistory = m_transientHistories[sessionId];
            transientHistory.append(userMsg);
            history = transientHistory;
        }

        m_currentOperationId = QStringLiteral("op_") + QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);

        core::logging::LoggingService::instance().info(core::logging::Category::LlmRequest, QStringLiteral("User message submitted"), {
            {QStringLiteral("op"), m_currentOperationId},
            {QStringLiteral("session"), sessionId},
            {QStringLiteral("charCount"), QString::number(trimmed.length())},
            {QStringLiteral("historySize"), QString::number(history.size())}
        });

        emit userMessageCreated(sessionId, userMsg);
        
        // 3. 冻结请求参数
        m_requestTemplate = {};
        m_requestTemplate.model = selected->requestModelId();
        m_requestTemplate.stream = true;
        m_requestTemplate.useWebSearch = useWebSearch;
        m_requestTemplate.useDeepThinking = useDeepThinking;
        m_requestTemplate.reasoningEffort = reasoningEffort;
        m_systemPrompt = systemPrompt;
        m_currentProvider = selected->provider;

        // 4. 清理旧任务并启动新任务
        cancelCurrent();
        m_currentSessionId = sessionId;
        m_replyBuffer.clear();
        m_thoughtBuffer.clear();

        // 5. 由 UseCase 确立流式 Assistant 消息实体的唯一生命周期 ID
        m_currentAssistantMessageId = QUuid::createUuid();
        domain::conversation::Message assistantPlaceholder;
        assistantPlaceholder.id = m_currentAssistantMessageId;
        assistantPlaceholder.role = domain::MessageRole::Assistant;
        assistantPlaceholder.status = domain::MessageStatus::Sending;
        assistantPlaceholder.createdAt = QDateTime::currentDateTime();
        emit assistantMessageStarted(sessionId, assistantPlaceholder);

        startRequest(provider, requestForHistory(history));
    }

    void SendMessageUseCase::startRequest(const domain::model::ModelProvider& provider,
                                          const domain::llm::ChatRequest& request) {
        m_currentOp = m_chatGateway->sendRequest(provider, request);
        if (!m_currentOp) {
            domain::llm::ChatError error;
            error.category = domain::llm::ChatErrorCategory::Provider;
            error.code = QStringLiteral("RequestStartFailed");
            error.message = QStringLiteral("Unable to start the model request.");
            error.userMessage = QStringLiteral("无法启动模型请求。");
            emit generationFailed(m_currentSessionId, error);
            m_currentSessionId.clear();
            return;
        }
        m_currentOp->setParent(this);
        connect(m_currentOp, &ports::IChatOperation::eventReceived,
                this, &SendMessageUseCase::onChatEventReceived);
    }

    domain::llm::ChatRequest SendMessageUseCase::requestForHistory(
        const QList<domain::conversation::Message>& history) const {
        domain::llm::ChatRequest request = m_requestTemplate;
        request.messages.clear();
        if (!m_systemPrompt.trimmed().isEmpty()) {
            request.messages.append({domain::MessageRole::System, m_systemPrompt});
        }
        for (const auto& msg : history) {
            if (msg.status != domain::MessageStatus::Sent) continue;
            domain::llm::ChatMessage llmMsg;
            llmMsg.role = msg.role;
            for (const auto& block : msg.blocks) {
                if (block.isText()) {
                    if (!llmMsg.content.isEmpty()) llmMsg.content += QLatin1Char('\n');
                    llmMsg.content += std::get<domain::conversation::TextBlock>(block.payload).text;
                }
            }
            if (!llmMsg.content.isEmpty()) request.messages.append(llmMsg);
        }
        return request;
    }

    domain::conversation::Message SendMessageUseCase::makeAssistantMessage() const {
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
        return message;
    }

    void SendMessageUseCase::saveMessage(const domain::conversation::Message& message) {
        if (m_currentSessionId.isEmpty()) return;
        if (!m_conversationService) {
            m_transientHistories[m_currentSessionId].append(message);
            return;
        }
        auto history = m_conversationService->loadMessages(m_currentSessionId);
        history.append(message);
        m_conversationService->saveMessages(m_currentSessionId, history);
    }

    void SendMessageUseCase::completeGeneration() {
        const QString sessionId = m_currentSessionId;
        if (m_currentOp) { m_currentOp->deleteLater(); m_currentOp = nullptr; }
        m_currentSessionId.clear();
        m_currentAssistantMessageId = {};
        emit generationFinished(sessionId);
    }

    void SendMessageUseCase::cancelCurrent() {
        const bool hadActiveOp = (m_currentOp != nullptr);
        if (m_currentOp) {
            m_currentOp->cancel();
            m_currentOp->deleteLater();
            m_currentOp = nullptr;
        }
        m_replyBuffer.clear();
        m_thoughtBuffer.clear();
        m_currentAssistantMessageId = {};
        if (hadActiveOp && !m_currentSessionId.isEmpty()) {
            emit generationFinished(m_currentSessionId);
        }
        m_currentSessionId.clear();
    }

    bool SendMessageUseCase::isGenerating() const {
        return m_currentOp != nullptr;
    }

    void SendMessageUseCase::onChatEventReceived(const domain::llm::ChatEvent &event) {
        if (m_currentSessionId.isEmpty()) return;
        
        std::visit([this](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            
            if constexpr (std::is_same_v<T, domain::llm::EventStarted>) {
                qWarning() << "[STREAM_TRACE][UseCase] EventStarted";
            } else if constexpr (std::is_same_v<T, domain::llm::EventTextDelta>) {
                m_replyBuffer += arg.text;
                qWarning() << "[STREAM_TRACE][UseCase] tokenReceived len=" << arg.text.length() << "total=" << m_replyBuffer.length();
                emit tokenReceived(m_currentSessionId, m_currentAssistantMessageId, arg.text);
            } else if constexpr (std::is_same_v<T, domain::llm::EventThinkingDelta>) {
                m_thoughtBuffer += arg.thought;
                qWarning() << "[STREAM_TRACE][UseCase] thoughtReceived len=" << arg.thought.length() << "total=" << m_thoughtBuffer.length();
                emit thoughtReceived(m_currentSessionId, m_currentAssistantMessageId, arg.thought);
            } else if constexpr (std::is_same_v<T, domain::llm::EventFinished>) {
                qWarning() << "[STREAM_TRACE][UseCase] EventFinished replyLen=" << m_replyBuffer.length() << "thoughtLen=" << m_thoughtBuffer.length();
                const auto assistantMsg = makeAssistantMessage();
                saveMessage(assistantMsg);
                emit replyGenerated(m_currentSessionId, assistantMsg);
                completeGeneration();
            } else if constexpr (std::is_same_v<T, domain::llm::EventError>) {
                core::logging::LoggingService::instance().warning(core::logging::Category::LlmRequest, QStringLiteral("LLM stream error"), QMap<QString, QString>{
                    {QStringLiteral("session"), m_currentSessionId},
                    {QStringLiteral("error"), arg.error.message},
                    {QStringLiteral("userMsg"), arg.error.userMessage}
                });
                bool isCancelled = (arg.error.category == domain::llm::ChatErrorCategory::Cancelled);
                
                if (!m_replyBuffer.isEmpty() || !m_thoughtBuffer.isEmpty()) {
                    domain::conversation::Message partialMsg;
                    partialMsg.id = m_currentAssistantMessageId.isNull() ? QUuid::createUuid() : m_currentAssistantMessageId;
                    partialMsg.role = domain::MessageRole::Assistant;
                    partialMsg.status = isCancelled ? domain::MessageStatus::Interrupted : domain::MessageStatus::Failed;
                    partialMsg.errorMessage = arg.error.userMessage.isEmpty() ? arg.error.message : arg.error.userMessage;
                    partialMsg.createdAt = QDateTime::currentDateTime();

                    if (!m_thoughtBuffer.isEmpty()) {
                        partialMsg.blocks.append(domain::conversation::MessageBlock(
                            domain::BlockType::Thought,
                            domain::conversation::ThoughtBlock{m_thoughtBuffer, 0}
                        ));
                    }
                    if (!m_replyBuffer.isEmpty()) {
                        partialMsg.blocks.append(domain::conversation::MessageBlock(
                            domain::BlockType::Text,
                            domain::conversation::TextBlock{m_replyBuffer}
                        ));
                    }
                    saveMessage(partialMsg);
                    emit replyGenerated(m_currentSessionId, partialMsg);
                }

                if (isCancelled) {
                    emit generationFinished(m_currentSessionId);
                } else {
                    emit generationFailed(m_currentSessionId, arg.error);
                }

                if (m_currentOp) { m_currentOp->deleteLater(); m_currentOp = nullptr; }
                m_currentSessionId.clear();
            }
        }, event);
    }

} // namespace application::usecase::chat
