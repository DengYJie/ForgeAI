#include "SendMessageUseCase.h"
#include "domain/service/IConversationService.h"
#include "domain/service/IModelService.h"
#include "domain/llm/ChatRequest.h"
#include <QDateTime>
#include <QUuid>

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

    void SendMessageUseCase::execute(const QString &sessionId, const QString &text) {
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
        
        // 假设这里我们总是使用选定的那个 provider (默认或从 settings/model service 拿)
        // MVP 阶段这里暂时硬编码获取第一个启用的 Provider
        domain::model::ModelProvider provider;
        auto providers = m_modelService->getActiveProviders();
        if (providers.isEmpty()) {
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Configuration;
            err.code = QStringLiteral("NoActiveProvider");
            err.message = QStringLiteral("No model provider configured.");
            err.userMessage = QStringLiteral("未配置或启用任何模型提供商，请在设置中添加。");
            err.suggestedAction = QStringLiteral("OpenSettings");
            emit generationFailed(sessionId, err);
            return;
        }
        provider = providers.first(); // 真实逻辑应是获取当前激活的 provider
        
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
            history.append(userMsg);
        }

        emit userMessageCreated(sessionId, userMsg);
        
        // 3. 构建 ChatRequest
        domain::llm::ChatRequest request;
        request.model = "gpt-4o"; // TODO: get from active model selection
        request.stream = true;
        
        // 将 conversation history 转换为 ChatMessage
        for (const auto &msg : history) {
            if (msg.status != domain::MessageStatus::Sent) continue;
            
            domain::llm::ChatMessage llmMsg;
            llmMsg.role = msg.role;
            
            QString content;
            for (const auto &block : msg.blocks) {
                if (std::holds_alternative<domain::conversation::TextBlock>(block.payload)) {
                    content += std::get<domain::conversation::TextBlock>(block.payload).text + "\n";
                } else if (std::holds_alternative<domain::conversation::ToolCallBlock>(block.payload)) {
                    llmMsg.toolCalls = std::get<domain::conversation::ToolCallBlock>(block.payload).calls;
                } else if (std::holds_alternative<domain::conversation::ToolResultBlock>(block.payload)) {
                    const auto &resBlock = std::get<domain::conversation::ToolResultBlock>(block.payload);
                    for (const auto &res : resBlock.results) {
                        domain::llm::ChatMessage toolResMsg;
                        toolResMsg.role = domain::MessageRole::Tool;
                        toolResMsg.toolCallId = res.toolCallId;
                        toolResMsg.content = res.content;
                        request.messages.append(toolResMsg);
                    }
                }
            }
            llmMsg.content = content.trimmed();
            if (!llmMsg.content.isEmpty() || llmMsg.toolCalls.has_value()) {
                request.messages.append(llmMsg);
            }
        }
        
        // 4. 清理旧任务并启动新任务
        cancelCurrent();
        m_currentSessionId = sessionId;
        m_replyBuffer.clear();
        m_thoughtBuffer.clear();
        m_activeToolCalls.clear();
        
        m_currentOp = m_chatGateway->sendRequest(provider, request);
        if (m_currentOp) {
            m_currentOp->setParent(this);
            connect(m_currentOp, &ports::IChatOperation::eventReceived, this, &SendMessageUseCase::onChatEventReceived);
        }
    }

    void SendMessageUseCase::cancelCurrent() {
        if (m_currentOp) {
            m_currentOp->cancel();
            m_currentOp->deleteLater();
            m_currentOp = nullptr;
        }
        m_replyBuffer.clear();
        m_thoughtBuffer.clear();
        m_activeToolCalls.clear();
        if (!m_currentSessionId.isEmpty()) {
            emit generationFinished(m_currentSessionId);
            m_currentSessionId.clear();
        }
    }

    bool SendMessageUseCase::isGenerating() const {
        return m_currentOp != nullptr;
    }

    void SendMessageUseCase::onChatEventReceived(const domain::llm::ChatEvent &event) {
        if (m_currentSessionId.isEmpty()) return;
        
        std::visit([this](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            
            if constexpr (std::is_same_v<T, domain::llm::EventStarted>) {
                // Ignore or handle
            } else if constexpr (std::is_same_v<T, domain::llm::EventTextDelta>) {
                m_replyBuffer += arg.text;
                emit tokenReceived(m_currentSessionId, arg.text);
            } else if constexpr (std::is_same_v<T, domain::llm::EventThinkingDelta>) {
                m_thoughtBuffer += arg.thought;
                emit thoughtReceived(m_currentSessionId, arg.thought);
            } else if constexpr (std::is_same_v<T, domain::llm::EventToolCallStarted>) {
                m_activeToolCalls[arg.id] = domain::agent::ToolCall{arg.id, arg.functionName, ""};
            } else if constexpr (std::is_same_v<T, domain::llm::EventToolCallDelta>) {
                if (m_activeToolCalls.contains(arg.id)) {
                    m_activeToolCalls[arg.id].arguments += arg.argumentsDelta;
                }
            } else if constexpr (std::is_same_v<T, domain::llm::EventToolCallFinished>) {
                if (m_activeToolCalls.contains(arg.id)) {
                    emit toolCallReceived(m_currentSessionId, m_activeToolCalls[arg.id]);
                }
            } else if constexpr (std::is_same_v<T, domain::llm::EventFinished>) {
                domain::conversation::Message assistantMsg;
                assistantMsg.id = QUuid::createUuid();
                assistantMsg.role = domain::MessageRole::Assistant;
                assistantMsg.status = domain::MessageStatus::Sent;
                assistantMsg.createdAt = QDateTime::currentDateTime();

                if (!m_thoughtBuffer.isEmpty()) {
                    assistantMsg.blocks.append(domain::conversation::MessageBlock(
                        domain::BlockType::Thought,
                        domain::conversation::ThoughtBlock{m_thoughtBuffer, 0}
                    ));
                }

                if (!m_replyBuffer.isEmpty()) {
                    assistantMsg.blocks.append(domain::conversation::MessageBlock(
                        domain::BlockType::Text,
                        domain::conversation::TextBlock{m_replyBuffer}
                    ));
                }

                if (!m_activeToolCalls.isEmpty()) {
                    domain::conversation::ToolCallBlock tcBlock;
                    tcBlock.calls = m_activeToolCalls.values();
                    assistantMsg.blocks.append(domain::conversation::MessageBlock(
                        domain::BlockType::ToolCall,
                        tcBlock
                    ));
                }

                if (m_conversationService) {
                    auto history = m_conversationService->loadMessages(m_currentSessionId);
                    history.append(assistantMsg);
                    m_conversationService->saveMessages(m_currentSessionId, history);
                }

                emit replyGenerated(m_currentSessionId, assistantMsg);
                emit generationFinished(m_currentSessionId);
                
                if (m_currentOp) {
                    m_currentOp->deleteLater();
                    m_currentOp = nullptr;
                }
                m_currentSessionId.clear();
            } else if constexpr (std::is_same_v<T, domain::llm::EventError>) {
                bool isCancelled = (arg.error.category == domain::llm::ChatErrorCategory::Cancelled);
                
                // 如果在被取消或报错前已经接收到部分生成内容，保存并保留给用户
                if (!m_replyBuffer.isEmpty() || !m_thoughtBuffer.isEmpty() || !m_activeToolCalls.isEmpty()) {
                    domain::conversation::Message partialMsg;
                    partialMsg.id = QUuid::createUuid();
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
                    if (!m_activeToolCalls.isEmpty()) {
                        domain::conversation::ToolCallBlock tcBlock;
                        tcBlock.calls = m_activeToolCalls.values();
                        partialMsg.blocks.append(domain::conversation::MessageBlock(
                            domain::BlockType::ToolCall,
                            tcBlock
                        ));
                    }

                    if (m_conversationService) {
                        auto history = m_conversationService->loadMessages(m_currentSessionId);
                        history.append(partialMsg);
                        m_conversationService->saveMessages(m_currentSessionId, history);
                    }

                    emit replyGenerated(m_currentSessionId, partialMsg);
                }

                if (isCancelled) {
                    emit generationFinished(m_currentSessionId);
                } else {
                    emit generationFailed(m_currentSessionId, arg.error);
                }

                if (m_currentOp) {
                    m_currentOp->deleteLater();
                    m_currentOp = nullptr;
                }
                m_currentSessionId.clear();
            }
        }, event);
    }

} // namespace application::usecase::chat
