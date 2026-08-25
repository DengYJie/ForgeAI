#include "ChatViewModel.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"
#include <QDateTime>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QJsonObject>
#include <algorithm>

namespace ui::screen::chat {
    namespace {
        QStringList canonicalReasoningEfforts(const QString& canonicalId) {
            static QHash<QString, QStringList> cache;
            static bool loaded = false;
            if (!loaded) {
                loaded = true;
                QFile file(QStringLiteral(":/config/models.json"));
                if (file.open(QIODevice::ReadOnly)) {
                    const QJsonObject models = QJsonDocument::fromJson(file.readAll()).object();
                    for (auto it = models.begin(); it != models.end(); ++it) {
                        QStringList values;
                        for (const auto& option : it.value().toObject().value(QStringLiteral("reasoning_options")).toArray()) {
                            const QJsonObject object = option.toObject();
                            if (object.value(QStringLiteral("type")).toString() != QStringLiteral("effort")) continue;
                            for (const auto& value : object.value(QStringLiteral("values")).toArray()) values.append(value.toString());
                        }
                        cache.insert(it.key(), values);
                    }
                }
            }
            return cache.value(canonicalId);
        }
        QString extractMessageText(const domain::conversation::Message &msg) {
            QString result;
            for (const auto &block : msg.blocks) {
                if (block.isText()) {
                    if (!result.isEmpty()) result += QLatin1Char('\n');
                    result += std::get<domain::conversation::TextBlock>(block.payload).text;
                }
            }
            return result;
        }

        // 返回 messages 中第一条 User 消息的文本前 18 字（+…）作为自动标题
        QString autoTitle(const QList<domain::conversation::Message> &messages) {
            for (const auto &msg : messages) {
                if (msg.role == domain::MessageRole::User) {
                    const QString text = extractMessageText(msg);
                    if (!text.trimmed().isEmpty()) {
                        return text.left(18) + (text.length() > 18 ? QStringLiteral("...") : QString());
                    }
                }
            }
            return QStringLiteral("新对话");
        }
    } // namespace

    ChatViewModel::ChatViewModel(
        const application::usecase::chat::ChatUseCases &useCases,
        QObject *parent
    ) : BaseViewModel<ChatViewModel, ChatState>(parent),
        m_useCases(useCases) {
        setupUseCaseConnections();

        updateState([this](ChatState &s) {
            if (m_useCases.loadSessions) {
                s.sessions = m_useCases.loadSessions->execute();
                s.sessions.erase(std::remove_if(s.sessions.begin(), s.sessions.end(),
                    [](const ChatSessionItemData& session) { return session.projectId.has_value(); }), s.sessions.end());
            }
            if (!s.sessions.isEmpty()) {
                s.currentSessionId = s.sessions.first().id;
                s.sessionTitle = s.sessions.first().title;
                s.sessionTitleManuallyEdited = (s.sessionTitle != QStringLiteral("新对话"));
                if (m_useCases.loadSessionDetail) {
                    s.messages = m_useCases.loadSessionDetail->execute(s.currentSessionId);
                }
            } else {
                s.currentSessionId = QUuid::createUuid().toString();
                s.sessionTitle = QStringLiteral("新对话");
            }
            refreshAvailableModels(s);
            recalculateAnchors(s);
        });
    }

    ChatViewModel::~ChatViewModel() = default;

    void ChatViewModel::setupUseCaseConnections() {
        if (m_useCases.getModels) {
            connect(m_useCases.getModels, &application::usecase::settings::GetModelsUseCase::modelsChanged, this, [this] {
                updateState([this](ChatState& state) { refreshAvailableModels(state); });
            });
        }
        if (!m_useCases.sendMessage) return;

        connect(m_useCases.sendMessage, &application::usecase::chat::SendMessageUseCase::userMessageCreated,
                this, [this](const QString &sessionId, const domain::conversation::Message &msg) {
            updateState([this, sessionId, msg](ChatState &s) {
                if (s.currentSessionId != sessionId) return;
                const bool isFirstMessage = s.messages.isEmpty();
                s.messages.append(msg);
                s.isGenerating = true;
                s.streamingMessageId = {};

                if (isFirstMessage && !s.sessionTitleManuallyEdited) {
                    const QString newTitle = autoTitle(s.messages);
                    s.sessionTitle = newTitle;
                    syncSessionTitle(s, s.currentSessionId, newTitle);
                }
                recalculateAnchors(s);
            });
        });

        connect(m_useCases.sendMessage, &application::usecase::chat::SendMessageUseCase::tokenReceived,
                this, [this](const QString& sessionId, const QString& token) {
            updateState([sessionId, token](ChatState& s) {
                if (s.currentSessionId != sessionId || token.isEmpty()) return;
                auto it = std::find_if(s.messages.begin(), s.messages.end(), [&](const auto& message) { return message.id == s.streamingMessageId; });
                if (it == s.messages.end()) {
                    domain::conversation::Message streaming;
                    streaming.id = QUuid::createUuid(); streaming.role = domain::MessageRole::Assistant;
                    streaming.status = domain::MessageStatus::Sending; streaming.createdAt = QDateTime::currentDateTime();
                    streaming.blocks.append({domain::BlockType::Text, domain::conversation::TextBlock{token}});
                    s.streamingMessageId = streaming.id; s.messages.append(std::move(streaming));
                    return;
                }
                for (auto& block : it->blocks) {
                    if (block.isText()) { std::get<domain::conversation::TextBlock>(block.payload).text += token; return; }
                }
                it->blocks.append({domain::BlockType::Text, domain::conversation::TextBlock{token}});
            });
        });

        connect(m_useCases.sendMessage, &application::usecase::chat::SendMessageUseCase::thoughtReceived,
                this, [this](const QString& sessionId, const QString& thought) {
            updateState([sessionId, thought](ChatState& s) {
                if (s.currentSessionId != sessionId || thought.isEmpty()) return;
                auto it = std::find_if(s.messages.begin(), s.messages.end(), [&](const auto& message) { return message.id == s.streamingMessageId; });
                if (it == s.messages.end()) {
                    domain::conversation::Message streaming;
                    streaming.id = QUuid::createUuid(); streaming.role = domain::MessageRole::Assistant;
                    streaming.status = domain::MessageStatus::Sending; streaming.createdAt = QDateTime::currentDateTime();
                    streaming.blocks.append({domain::BlockType::Thought, domain::conversation::ThoughtBlock{thought, 0}});
                    s.streamingMessageId = streaming.id; s.messages.append(std::move(streaming));
                    return;
                }
                for (auto& block : it->blocks) {
                    if (block.isThought()) { std::get<domain::conversation::ThoughtBlock>(block.payload).thought += thought; return; }
                }
                it->blocks.append({domain::BlockType::Thought, domain::conversation::ThoughtBlock{thought, 0}});
            });
        });

        connect(m_useCases.sendMessage, &application::usecase::chat::SendMessageUseCase::replyGenerated,
                this, [this](const QString &sessionId, const domain::conversation::Message &msg) {
            core::logging::LoggingService::instance().info(core::logging::Category::LlmRequest, QStringLiteral("ChatViewModel replyGenerated"), QMap<QString, QString>{
                {QStringLiteral("session"), sessionId},
                {QStringLiteral("blocks"), QString::number(msg.blocks.size())}
            });
            updateState([sessionId, msg](ChatState &s) {
                if (s.currentSessionId != sessionId) return;
                const auto streaming = std::find_if(s.messages.begin(), s.messages.end(), [&](const auto& message) { return message.id == s.streamingMessageId; });
                if (streaming != s.messages.end()) *streaming = msg;
                else s.messages.append(msg);
                s.streamingMessageId = {};
                recalculateAnchors(s);
            });
        });

        connect(m_useCases.sendMessage, &application::usecase::chat::SendMessageUseCase::generationFinished,
                this, [this](const QString &sessionId) {
            core::logging::LoggingService::instance().info(core::logging::Category::LlmRequest, QStringLiteral("ChatViewModel generationFinished"), QMap<QString, QString>{
                {QStringLiteral("session"), sessionId}
            });
            updateState([sessionId](ChatState &s) {
                if (s.currentSessionId == sessionId) {
                    s.isGenerating = false;
                    s.streamingMessageId = {};
                    s.statusMessage.clear();
                    s.lastError.reset();
                }
            });
        });

        connect(m_useCases.sendMessage, &application::usecase::chat::SendMessageUseCase::generationFailed,
                this, [this](const QString &sessionId, const domain::llm::ChatError &error) {
            core::logging::LoggingService::instance().warning(core::logging::Category::LlmRequest, QStringLiteral("ChatViewModel generationFailed"), QMap<QString, QString>{
                {QStringLiteral("session"), sessionId},
                {QStringLiteral("error"), error.message}
            });
            updateState([sessionId, error](ChatState &s) {
                if (s.currentSessionId == sessionId) {
                    s.isGenerating = false;
                    s.streamingMessageId = {};
                    s.lastError = error;
                    s.statusMessage = error.userMessage.isEmpty() ? error.message : error.userMessage;
                }
            });
        });
    }

    void ChatViewModel::loadSession(const QString &sessionId) {
        updateState([this, sessionId](ChatState &s) {
            QString title = QStringLiteral("新对话");
            bool manuallyEdited = false;
            for (const auto &sess : s.sessions) {
                if (sess.id == sessionId) {
                    title = sess.title;
                    manuallyEdited = (title != QStringLiteral("新对话"));
                    break;
                }
            }
            s.currentSessionId = sessionId;
            s.sessionTitle = title;
            s.sessionTitleManuallyEdited = manuallyEdited;
            s.isGenerating = false;
            if (m_useCases.loadSessionDetail) {
                s.messages = m_useCases.loadSessionDetail->execute(sessionId);
            } else {
                s.messages.clear();
            }
            recalculateAnchors(s);
        });
    }

    void ChatViewModel::newSession() {
        updateState([this](ChatState &s) {
            if (m_useCases.createSession) {
                const QString targetId = m_useCases.createSession->execute(s.sessions, s.currentSessionId);
                if (targetId == s.currentSessionId && s.messages.isEmpty()) {
                    return;
                }
                s.currentSessionId = targetId;
                s.sessionTitle = QStringLiteral("新对话");
                s.sessionTitleManuallyEdited = false;
                if (m_useCases.loadSessionDetail) {
                    s.messages = m_useCases.loadSessionDetail->execute(targetId);
                } else {
                    s.messages.clear();
                }
                s.isGenerating = false;
                recalculateAnchors(s);
            }
        });
    }

    void ChatViewModel::deleteSession(const QString &sessionId) {
        updateState([this, sessionId](ChatState &s) {
            if (m_useCases.deleteSession) {
                const QString nextId = m_useCases.deleteSession->execute(s.sessions, sessionId, s.currentSessionId);
                if (s.currentSessionId == sessionId) {
                    s.currentSessionId = nextId;
                    QString title = QStringLiteral("新对话");
                    for (const auto &sess : s.sessions) {
                        if (sess.id == nextId) {
                            title = sess.title;
                            break;
                        }
                    }
                    s.sessionTitle = title;
                    s.sessionTitleManuallyEdited = (title != QStringLiteral("新对话"));
                    if (m_useCases.loadSessionDetail) {
                        s.messages = m_useCases.loadSessionDetail->execute(nextId);
                    } else {
                        s.messages.clear();
                    }
                    s.isGenerating = false;
                    recalculateAnchors(s);
                }
            }
        });
    }

    void ChatViewModel::sendMessage(const QString &text) {
        const QString trimmed = text.trimmed();
        if (trimmed.isEmpty()) return;

        if (m_useCases.sendMessage) {
            m_useCases.sendMessage->execute(m_state.currentSessionId, trimmed,
                                            m_state.currentModelProviderId, m_state.currentModelId,
                                            m_state.useWebSearch, m_state.useDeepThinking, m_state.reasoningEffort);
        }
    }

    void ChatViewModel::stopGeneration() {
        if (m_useCases.stopGeneration) {
            m_useCases.stopGeneration->execute();
        }
        updateState([](ChatState &s) {
            s.isGenerating = false;
            s.statusMessage.clear();
            s.lastError.reset();
        });
    }

    void ChatViewModel::clearCurrentSession() {
        stopGeneration();
        updateState([this](ChatState& s) {
            if (m_useCases.clearSession) m_useCases.clearSession->execute(s.currentSessionId);
            s.messages.clear();
            s.streamingMessageId = {};
            s.statusMessage.clear();
            s.lastError.reset();
            recalculateAnchors(s);
        });
    }

    void ChatViewModel::setSessionPinned(const QString& sessionId, bool pinned) {
        updateState([this, sessionId, pinned](ChatState& s) {
            if (m_useCases.setSessionPinned) m_useCases.setSessionPinned->execute(s.sessions, sessionId, pinned);
        });
    }

    void ChatViewModel::setSessionArchived(const QString& sessionId, bool archived) {
        updateState([this, sessionId, archived](ChatState& s) {
            if (m_useCases.setSessionArchived)
                m_useCases.setSessionArchived->execute(s.sessions, sessionId, archived);
            if (archived && s.currentSessionId == sessionId) {
                if (s.isGenerating) {
                    QTimer::singleShot(0, this, &ChatViewModel::stopGeneration);
                }
                const auto next = std::find_if(s.sessions.cbegin(), s.sessions.cend(),
                    [](const ChatSessionItemData& item) { return !item.isArchived; });
                if (next != s.sessions.cend()) {
                    s.currentSessionId = next->id;
                    s.sessionTitle = next->title;
                    s.sessionTitleManuallyEdited = next->title != QStringLiteral("新对话");
                    s.messages = m_useCases.loadSessionDetail
                        ? m_useCases.loadSessionDetail->execute(next->id)
                        : QList<domain::conversation::Message>{};
                } else if (m_useCases.createSession) {
                    const QString newId = m_useCases.createSession->execute(s.sessions, {});
                    s.currentSessionId = newId;
                    s.sessionTitle = QStringLiteral("新对话");
                    s.sessionTitleManuallyEdited = false;
                    s.messages.clear();
                }
                s.isGenerating = false;
                recalculateAnchors(s);
            }
        });
    }

    void ChatViewModel::setWebSearchEnabled(bool enabled) {
        updateState([enabled](ChatState& state) { state.useWebSearch = enabled; });
    }

    void ChatViewModel::setDeepThinkingEnabled(bool enabled) {
        updateState([enabled](ChatState& state) { state.useDeepThinking = enabled; });
    }

    void ChatViewModel::setReasoningEffort(const QString& effort) {
        updateState([effort](ChatState& state) {
            state.reasoningEffort = effort;
            state.useDeepThinking = !effort.isEmpty() && effort != QStringLiteral("none");
        });
    }

    void ChatViewModel::setModelName(const QString &modelName) {
        updateState([modelName](ChatState &s) {
            const auto it = std::find_if(s.availableModels.cbegin(), s.availableModels.cend(), [&](const ChatModelOption& option) {
                return option.displayName == modelName;
            });
            if (it != s.availableModels.cend()) {
                s.currentModelName = it->displayName;
                s.currentModelProviderId = it->providerId;
                s.currentModelId = it->modelId;
            }
        });
    }

    void ChatViewModel::setModel(const QString &providerId, const QString &modelId) {
        updateState([providerId, modelId](ChatState &s) {
            const auto it = std::find_if(s.availableModels.cbegin(), s.availableModels.cend(), [&](const ChatModelOption& option) {
                return option.providerId == providerId && option.modelId == modelId;
            });
            if (it == s.availableModels.cend()) return;
            s.currentModelProviderId = it->providerId;
            s.currentModelId = it->modelId;
            s.currentModelName = it->displayName;
            s.reasoningEffort = it->reasoningEfforts.contains(QStringLiteral("medium"))
                ? QStringLiteral("medium") : (it->reasoningEfforts.isEmpty() ? QString() : it->reasoningEfforts.first());
        });
    }

    void ChatViewModel::setActiveAnchorByMessageId(const QUuid &messageId) {
        updateState([messageId](ChatState &s) {
            int anchorIdx = 0;
            for (const auto &msg : s.messages) {
                if (msg.role == domain::MessageRole::User) {
                    if (msg.id == messageId) {
                        s.activeAnchorIndex = anchorIdx;
                        return;
                    }
                    ++anchorIdx;
                }
            }
        });
    }

    void ChatViewModel::setActiveAnchorIndex(int index) {
        updateState([index](ChatState &s) {
            s.activeAnchorIndex = index;
        });
    }

    void ChatViewModel::refreshAvailableModels(ChatState &s) const {
        s.availableModels.clear();
        if (!m_useCases.getModels) return;
        const auto models = m_useCases.getModels->getEnabledResolvedModels();
        for (const auto& model : models) {
            const auto capabilities = model.effectiveCapabilities();
            QStringList efforts;
            const QJsonArray options = QJsonDocument::fromJson(model.binding.reasoningOptionsJson.toUtf8()).array();
            for (const auto& option : options) {
                const QJsonObject object = option.toObject();
                if (object.value(QStringLiteral("type")).toString() != QStringLiteral("effort")) continue;
                for (const auto& value : object.value(QStringLiteral("values")).toArray()) efforts.append(value.toString());
            }
            if (efforts.isEmpty() && model.binding.canonicalModelId.has_value())
                efforts = canonicalReasoningEfforts(*model.binding.canonicalModelId);
            s.availableModels.append({
                model.provider.id, model.requestModelId(), model.displayName(), model.provider.name,
                capabilities.testFlag(domain::model::ModelCapability::Vision)
                    || capabilities.testFlag(domain::model::ModelCapability::Pdf)
                    || capabilities.testFlag(domain::model::ModelCapability::Audio)
                    || capabilities.testFlag(domain::model::ModelCapability::Video),
                model.provider.type == domain::model::ProviderType::OpenAIResponses,
                capabilities.testFlag(domain::model::ModelCapability::Thinking), efforts
            });
        }
        if (s.availableModels.isEmpty()) {
            s.currentModelProviderId.clear();
            s.currentModelId.clear();
            s.currentModelName = QStringLiteral("选择模型");
            return;
        }
        const auto current = std::find_if(s.availableModels.cbegin(), s.availableModels.cend(), [&](const ChatModelOption& option) {
            return option.providerId == s.currentModelProviderId && option.modelId == s.currentModelId;
        });
        if (current == s.availableModels.cend()) {
            const ChatModelOption& fallback = s.availableModels.first();
            s.currentModelProviderId = fallback.providerId;
            s.currentModelId = fallback.modelId;
            s.currentModelName = fallback.displayName;
        }
    }

    void ChatViewModel::recalculateAnchors(ChatState &s) {
        s.anchors.clear();
        for (qsizetype i = 0; i < s.messages.size(); ++i) {
            const auto &msg = s.messages[i];
            if (msg.role == domain::MessageRole::User) {
                const QString userText = extractMessageText(msg);
                const QString title = userText.left(18) + (userText.length() > 18 ? QStringLiteral("...") : QString());

                // 提取对应的下一条 Assistant 消息的前 60 字符，作为悬停预览卡片的内容
                QString previewText;
                for (qsizetype j = i + 1; j < s.messages.size(); ++j) {
                    if (s.messages[j].role == domain::MessageRole::Assistant) {
                        const QString assistantText = extractMessageText(s.messages[j]);
                        previewText = assistantText.left(60) + (assistantText.length() > 60 ? QStringLiteral("...") : QString());
                        break;
                    }
                    if (s.messages[j].role == domain::MessageRole::User) break;
                }

                s.anchors.append({msg.id.toString(), title, previewText});
            }
        }
        if (s.activeAnchorIndex < 0 || s.activeAnchorIndex >= s.anchors.size()) {
            s.activeAnchorIndex = s.anchors.isEmpty() ? -1 : (s.anchors.size() - 1);
        }
    }

    void ChatViewModel::syncSessionTitle(ChatState &s, const QString &sessionId, const QString &title) const {
        if (m_useCases.setSessionTitle) {
            m_useCases.setSessionTitle->execute(s.sessions, sessionId, title);
            return;
        }
        for (auto &session : s.sessions) {
            if (session.id == sessionId) { session.title = title; break; }
        }
    }

    void ChatViewModel::emitStateChanged() {
        Q_EMIT stateChanged(m_state);
    }
} // namespace ui::screen::chat
