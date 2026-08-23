#include "ChatViewModel.h"
#include "domain/service/IChatService.h"
#include "domain/service/IConversationService.h"
#include <QDateTime>

namespace ui::screen::chat {
    namespace {
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
        domain::service::IChatService *chatService,
        domain::service::IConversationService *conversationService,
        QObject *parent
    ) : BaseViewModel<ChatViewModel, ChatState>(parent),
        m_chatService(chatService),
        m_conversationService(conversationService) {
        setupServiceConnections();

        updateState([this](ChatState &s) {
            if (m_conversationService) {
                s.sessions = m_conversationService->loadSessions();
            }
            if (!s.sessions.isEmpty()) {
                s.currentSessionId = s.sessions.first().id;
                s.sessionTitle = s.sessions.first().title;
                s.sessionTitleManuallyEdited = (s.sessionTitle != QStringLiteral("新对话"));
                if (m_conversationService) {
                    s.messages = m_conversationService->loadMessages(s.currentSessionId);
                }
            } else {
                s.currentSessionId = QStringLiteral("session_1");
                s.sessionTitle = QStringLiteral("新对话");
            }
            recalculateAnchors(s);
        });
    }

    ChatViewModel::~ChatViewModel() = default;

    void ChatViewModel::setupServiceConnections() {
        if (!m_chatService) return;

        connect(m_chatService, &domain::service::IChatService::messageGenerated, this, [this](const QString &sessionId, const QString &fullReply) {
            updateState([this, sessionId, fullReply](ChatState &s) {
                if (s.currentSessionId != sessionId) return;

                domain::conversation::Message assistantMsg;
                assistantMsg.id = QUuid::createUuid();
                assistantMsg.role = domain::MessageRole::Assistant;
                assistantMsg.status = domain::MessageStatus::Sent;
                assistantMsg.createdAt = QDateTime::currentDateTime();
                assistantMsg.blocks.append(domain::conversation::MessageBlock(domain::BlockType::Text, domain::conversation::TextBlock{fullReply}));

                s.messages.append(assistantMsg);
                if (m_conversationService) {
                    m_conversationService->saveMessages(s.currentSessionId, s.messages);
                }
                recalculateAnchors(s);
            });
        });

        connect(m_chatService, &domain::service::IChatService::generationFinished, this, [this](const QString &sessionId) {
            updateState([sessionId](ChatState &s) {
                if (s.currentSessionId == sessionId) {
                    s.isGenerating = false;
                    s.statusMessage.clear();
                }
            });
        });

        connect(m_chatService, &domain::service::IChatService::generationFailed, this, [this](const QString &sessionId, const QString &errorMessage) {
            updateState([sessionId, errorMessage](ChatState &s) {
                if (s.currentSessionId == sessionId) {
                    s.isGenerating = false;
                    s.statusMessage = errorMessage;
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
            if (m_conversationService) {
                s.messages = m_conversationService->loadMessages(sessionId);
            } else {
                s.messages.clear();
            }
            recalculateAnchors(s);
        });
    }

    void ChatViewModel::newSession() {
        updateState([this](ChatState &s) {
            if (m_conversationService) {
                const QString targetId = m_conversationService->reuseOrCreateSession(s.sessions, s.currentSessionId);
                if (targetId == s.currentSessionId && s.messages.isEmpty()) {
                    return;
                }
                s.currentSessionId = targetId;
                s.sessionTitle = QStringLiteral("新对话");
                s.sessionTitleManuallyEdited = false;
                s.messages = m_conversationService->loadMessages(targetId);
                s.isGenerating = false;
                recalculateAnchors(s);
            }
        });
    }

    void ChatViewModel::deleteSession(const QString &sessionId) {
        updateState([this, sessionId](ChatState &s) {
            if (m_conversationService) {
                const QString nextId = m_conversationService->deleteSession(s.sessions, sessionId, s.currentSessionId);
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
                    s.messages = m_conversationService->loadMessages(nextId);
                    s.isGenerating = false;
                    recalculateAnchors(s);
                }
            }
        });
    }

    void ChatViewModel::sendMessage(const QString &text) {
        const QString trimmed = text.trimmed();
        if (trimmed.isEmpty()) return;

        domain::conversation::Message userMsg;
        userMsg.id = QUuid::createUuid();
        userMsg.role = domain::MessageRole::User;
        userMsg.status = domain::MessageStatus::Sent;
        userMsg.createdAt = QDateTime::currentDateTime();
        userMsg.blocks.append(domain::conversation::MessageBlock(domain::BlockType::Text, domain::conversation::TextBlock{trimmed}));

        QString currentSess;
        updateState([&](ChatState &s) {
            const bool isFirstMessage = s.messages.isEmpty();
            s.messages.append(userMsg);
            s.isGenerating = true;
            currentSess = s.currentSessionId;

            if (isFirstMessage && !s.sessionTitleManuallyEdited) {
                const QString newTitle = autoTitle(s.messages);
                s.sessionTitle = newTitle;
                syncSessionTitle(s, s.currentSessionId, newTitle);
            }
            if (m_conversationService) {
                m_conversationService->saveMessages(s.currentSessionId, s.messages);
            }
            recalculateAnchors(s);
        });

        if (m_chatService) {
            m_chatService->sendMessage(currentSess, trimmed);
        }
    }

    void ChatViewModel::stopGeneration() {
        if (m_chatService) {
            m_chatService->stopGeneration();
        }
        updateState([](ChatState &s) {
            s.isGenerating = false;
            s.statusMessage.clear();
        });
    }

    void ChatViewModel::setModelName(const QString &modelName) {
        updateState([modelName](ChatState &s) {
            s.currentModelName = modelName;
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

    void ChatViewModel::syncSessionTitle(ChatState &s, const QString &sessionId, const QString &title) {
        for (auto &sess : s.sessions) {
            if (sess.id == sessionId) {
                sess.title = title;
                return;
            }
        }
    }

    void ChatViewModel::emitStateChanged() {
        Q_EMIT stateChanged(m_state);
    }
} // namespace ui::screen::chat
