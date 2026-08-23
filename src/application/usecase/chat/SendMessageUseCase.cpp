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
            emit generationFailed(sessionId, "System dependencies are not ready.");
            return;
        }
        
        // 假设这里我们总是使用选定的那个 provider (默认或从 settings/model service 拿)
        // MVP 阶段这里暂时硬编码获取第一个启用的 Provider
        domain::model::ModelProvider provider;
        auto providers = m_modelService->getActiveProviders();
        if (providers.isEmpty()) {
            emit generationFailed(sessionId, "No model provider configured.");
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
            // 简单将所有 TextBlock 拼起来
            QString content;
            for (const auto &block : msg.blocks) {
                if (std::holds_alternative<domain::conversation::TextBlock>(block.payload)) {
                    content += std::get<domain::conversation::TextBlock>(block.payload).text + "\n";
                }
            }
            llmMsg.content = content.trimmed();
            if (!llmMsg.content.isEmpty()) {
                request.messages.append(llmMsg);
            }
        }
        
        // 4. 清理旧任务并启动新任务
        cancelCurrent();
        m_currentSessionId = sessionId;
        m_replyBuffer.clear();
        
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
            } else if constexpr (std::is_same_v<T, domain::llm::EventFinished>) {
                domain::conversation::Message assistantMsg;
                assistantMsg.id = QUuid::createUuid();
                assistantMsg.role = domain::MessageRole::Assistant;
                assistantMsg.status = domain::MessageStatus::Sent;
                assistantMsg.createdAt = QDateTime::currentDateTime();
                assistantMsg.blocks.append(domain::conversation::MessageBlock(
                    domain::BlockType::Text,
                    domain::conversation::TextBlock{m_replyBuffer}
                ));

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
                emit generationFailed(m_currentSessionId, arg.error.message);
                if (m_currentOp) {
                    m_currentOp->deleteLater();
                    m_currentOp = nullptr;
                }
                m_currentSessionId.clear();
            }
        }, event);
    }

} // namespace application::usecase::chat
