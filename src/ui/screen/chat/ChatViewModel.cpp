#include "ChatViewModel.h"
#include <QDateTime>

namespace ui::screen::chat {
    namespace {
        QString extractMessageText(const domain::conversation::Message &msg) {
            QString result;
            for (const auto &block : msg.blocks) {
                if (block.isText()) {
                    if (!result.isEmpty()) {
                        result += QLatin1Char('\n');
                    }
                    result += std::get<domain::conversation::TextBlock>(block.payload).text;
                }
            }
            return result;
        }
    } // namespace

    ChatViewModel::ChatViewModel(QObject *parent)
        : BaseViewModel<ChatViewModel, ChatState>(parent) {
        updateState([](ChatState &s) {
            s.currentSessionId = QStringLiteral("session_1");
            s.sessionTitle = QStringLiteral("ForgeAI 架构与设计讨论");

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
        updateState([sessionId](ChatState &s) {
            s.currentSessionId = sessionId;
            s.sessionTitle = QStringLiteral("会话 %1").arg(sessionId);
            // 切换会话清空并重新初始化
            s.messages.clear();
            s.isGenerating = false;
            recalculateAnchors(s);
        });
    }

    void ChatViewModel::newSession() {
        static int newSessionIdx = 1;
        loadSession(QStringLiteral("session_%1").arg(++newSessionIdx));
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
            domain::conversation::TextBlock{QStringLiteral("收到您的提问：“%1”。系统已完成处理并实时更新了侧边对话时间线。").arg(trimmed)}
        ));

        updateState([userMsg, assistantMsg](ChatState &s) {
            s.messages.append(userMsg);
            s.messages.append(assistantMsg);
            s.isGenerating = false;
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
                    if (s.messages[j].role == domain::MessageRole::User) {
                        break;
                    }
                }

                s.anchors.append({msg.id.toString(), title, previewText});
            }
        }
        if (s.activeAnchorIndex < 0 || s.activeAnchorIndex >= s.anchors.size()) {
            s.activeAnchorIndex = s.anchors.isEmpty() ? -1 : (s.anchors.size() - 1);
        }
    }

    void ChatViewModel::emitStateChanged() {
        Q_EMIT stateChanged(m_state);
    }
} // namespace ui::screen::chat
