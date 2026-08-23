#include "ChatViewModel.h"
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

        ChatSessionItemData makeSession(const QString &id, const QString &title, bool isPinned = false) {
            ChatSessionItemData s;
            s.id = id;
            s.title = title;
            s.isPinned = isPinned;
            s.timestamp = QDateTime::currentMSecsSinceEpoch();
            return s;
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

    ChatViewModel::ChatViewModel(QObject *parent)
        : BaseViewModel<ChatViewModel, ChatState>(parent) {
        updateState([](ChatState &s) {
            // 初始化演示会话列表
            s.sessions.append(makeSession(QStringLiteral("session_1"), QStringLiteral("ForgeAI 架构与设计讨论"), true));
            s.sessions.append(makeSession(QStringLiteral("session_2"), QStringLiteral("新对话")));
            s.currentSessionId = QStringLiteral("session_1");
            s.sessionTitle = QStringLiteral("ForgeAI 架构与设计讨论");
            s.sessionTitleManuallyEdited = true; // 演示数据视为已命名

            domain::conversation::Message u1;
            u1.id = QUuid::createUuid();
            u1.role = domain::MessageRole::User;
            u1.status = domain::MessageStatus::Sent;
            u1.createdAt = QDateTime::currentDateTime();
            u1.blocks.append(domain::conversation::MessageBlock(domain::BlockType::Text, domain::conversation::TextBlock{QStringLiteral("如何使用 CMakeLists 结构优化及库依赖链接？")}));
            s.messages.append(u1);

            domain::conversation::Message a1;
            a1.id = QUuid::createUuid();
            a1.role = domain::MessageRole::Assistant;
            a1.status = domain::MessageStatus::Sent;
            a1.createdAt = QDateTime::currentDateTime();
            a1.blocks.append(domain::conversation::MessageBlock(domain::BlockType::Text, domain::conversation::TextBlock{QStringLiteral("通过使用 FetchContent 精细控制，能够高效集成第三方库并避免多余构建。")}));
            s.messages.append(a1);

            domain::conversation::Message u2;
            u2.id = QUuid::createUuid();
            u2.role = domain::MessageRole::User;
            u2.status = domain::MessageStatus::Sent;
            u2.createdAt = QDateTime::currentDateTime();
            u2.blocks.append(domain::conversation::MessageBlock(domain::BlockType::Text, domain::conversation::TextBlock{QStringLiteral("ForgeAI 的 UI 设计方案与组件选型")}));
            s.messages.append(u2);

            domain::conversation::Message a2;
            a2.id = QUuid::createUuid();
            a2.role = domain::MessageRole::Assistant;
            a2.status = domain::MessageStatus::Sent;
            a2.createdAt = QDateTime::currentDateTime();
            a2.blocks.append(domain::conversation::MessageBlock(domain::BlockType::Text, domain::conversation::TextBlock{QStringLiteral("主界面采用 Fluent Design 2 规范，使用纯自绘 FlatExpander、Card 消息气泡与 QVBoxLayout 原生流式排版。")}));
            s.messages.append(a2);

            domain::conversation::Message u3;
            u3.id = QUuid::createUuid();
            u3.role = domain::MessageRole::User;
            u3.status = domain::MessageStatus::Sent;
            u3.createdAt = QDateTime::currentDateTime();
            u3.blocks.append(domain::conversation::MessageBlock(domain::BlockType::Text, domain::conversation::TextBlock{QStringLiteral("ChatHeader、ChatAnchorBar 和 ChatInputBox 的落地")}));
            s.messages.append(u3);

            domain::conversation::Message a3;
            a3.id = QUuid::createUuid();
            a3.role = domain::MessageRole::Assistant;
            a3.status = domain::MessageStatus::Sent;
            a3.createdAt = QDateTime::currentDateTime();
            a3.blocks.append(domain::conversation::MessageBlock(domain::BlockType::Text, domain::conversation::TextBlock{QStringLiteral("完成了通用组件解耦，并实现了 ChatAnchorBar 与 MessageListView 的瞬时直达定位与双向高亮同步。")}));
            s.messages.append(a3);

            recalculateAnchors(s);
        });
    }

    ChatViewModel::~ChatViewModel() = default;

    void ChatViewModel::loadSession(const QString &sessionId) {
        // 切换到已存在的会话（此处简化：清空消息，真实场景从存储加载）
        updateState([sessionId](ChatState &s) {
            // 找到 sessions 中对应条目的标题
            QString title = QStringLiteral("新对话");
            bool manuallyEdited = false;
            for (const auto &sess : s.sessions) {
                if (sess.id == sessionId) {
                    title = sess.title;
                    // 若标题与默认标题相同，视为未命名
                    manuallyEdited = (title != QStringLiteral("新对话"));
                    break;
                }
            }
            s.currentSessionId = sessionId;
            s.sessionTitle = title;
            s.sessionTitleManuallyEdited = manuallyEdited;
            s.messages.clear();
            s.isGenerating = false;
            recalculateAnchors(s);
        });
    }

    void ChatViewModel::newSession() {
        updateState([](ChatState &s) {
            // ① 在现有会话列表中查找可复用的空白会话（无消息且标题未手动编辑）
            for (const auto &sess : s.sessions) {
                if (sess.title == QStringLiteral("新对话") && sess.id != s.currentSessionId) {
                    // 已存在空白占位，直接切换
                    s.currentSessionId = sess.id;
                    s.sessionTitle = QStringLiteral("新对话");
                    s.sessionTitleManuallyEdited = false;
                    s.messages.clear();
                    s.isGenerating = false;
                    recalculateAnchors(s);
                    return;
                }
            }

            // ② 若当前会话本身就是空白，什么都不做
            if (s.messages.isEmpty() && !s.sessionTitleManuallyEdited) {
                return;
            }

            // ③ 没有可复用的空白会话 → 创建全新会话
            static int idCounter = 100;
            const QString newId = QStringLiteral("session_%1").arg(++idCounter);
            const ChatSessionItemData newSess = makeSession(newId, QStringLiteral("新对话"));
            s.sessions.prepend(newSess); // 新会话置顶显示

            s.currentSessionId = newId;
            s.sessionTitle = QStringLiteral("新对话");
            s.sessionTitleManuallyEdited = false;
            s.messages.clear();
            s.isGenerating = false;
            recalculateAnchors(s);
        });
    }

    void ChatViewModel::deleteSession(const QString &sessionId) {
        updateState([sessionId](ChatState &s) {
            const int idx = [&]() {
                for (int i = 0; i < s.sessions.size(); ++i) {
                    if (s.sessions[i].id == sessionId) return i;
                }
                return -1;
            }();
            if (idx < 0) return;

            s.sessions.removeAt(idx);

            if (s.currentSessionId != sessionId) return;

            // 被删除的是当前会话 → 回退到邻近会话
            if (s.sessions.isEmpty()) {
                // 列表清空 → 自动创建新空白会话
                static int idCounter = 200;
                const QString newId = QStringLiteral("session_%1").arg(++idCounter);
                s.sessions.append(makeSession(newId, QStringLiteral("新对话")));
                s.currentSessionId = newId;
                s.sessionTitle = QStringLiteral("新对话");
                s.sessionTitleManuallyEdited = false;
            } else {
                // 选中下一个，若 idx 超界则选最后一个
                const int fallback = qMin(idx, s.sessions.size() - 1);
                s.currentSessionId = s.sessions[fallback].id;
                s.sessionTitle = s.sessions[fallback].title;
                s.sessionTitleManuallyEdited = (s.sessionTitle != QStringLiteral("新对话"));
            }
            s.messages.clear();
            s.isGenerating = false;
            recalculateAnchors(s);
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

        domain::conversation::Message assistantMsg;
        assistantMsg.id = QUuid::createUuid();
        assistantMsg.role = domain::MessageRole::Assistant;
        assistantMsg.status = domain::MessageStatus::Sent;
        assistantMsg.createdAt = QDateTime::currentDateTime();
        assistantMsg.blocks.append(domain::conversation::MessageBlock(
            domain::BlockType::Text,
            domain::conversation::TextBlock{QStringLiteral("收到您的提问：\"%1\"。系统已完成处理并实时更新了侧边对话时间线。").arg(trimmed)}
        ));

        updateState([userMsg, assistantMsg](ChatState &s) {
            const bool isFirstMessage = s.messages.isEmpty();

            s.messages.append(userMsg);
            s.messages.append(assistantMsg);
            s.isGenerating = false;

            // 首条消息且标题未手动改过 → 自动以用户文本前 18 字更新会话标题
            if (isFirstMessage && !s.sessionTitleManuallyEdited) {
                const QString newTitle = autoTitle(s.messages);
                s.sessionTitle = newTitle;
                syncSessionTitle(s, s.currentSessionId, newTitle);
            }

            recalculateAnchors(s);
        });
    }

    void ChatViewModel::stopGeneration() {
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
