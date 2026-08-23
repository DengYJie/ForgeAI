#include "ConversationService.h"
#include <QDateTime>
#include <QUuid>

namespace services::conversation {
    namespace {
        ui::screen::chat::ChatSessionItemData makeSession(const QString &id, const QString &title, bool isPinned = false) {
            ui::screen::chat::ChatSessionItemData s;
            s.id = id;
            s.title = title;
            s.isPinned = isPinned;
            s.timestamp = QDateTime::currentMSecsSinceEpoch();
            return s;
        }
    } // namespace

    ConversationService::ConversationService(
        domain::repository::IConversationRepository *conversationRepo,
        QObject *parent
    ) : IConversationService(parent), m_conversationRepo(conversationRepo) {
        // 初始化演示会话历史数据
        QList<domain::conversation::Message> initialMessages;

        domain::conversation::Message u1;
        u1.id = QUuid::createUuid();
        u1.role = domain::MessageRole::User;
        u1.status = domain::MessageStatus::Sent;
        u1.createdAt = QDateTime::currentDateTime();
        u1.blocks.append(domain::conversation::MessageBlock(domain::BlockType::Text, domain::conversation::TextBlock{QStringLiteral("如何使用 CMakeLists 结构优化及库依赖链接？")}));
        initialMessages.append(u1);

        domain::conversation::Message a1;
        a1.id = QUuid::createUuid();
        a1.role = domain::MessageRole::Assistant;
        a1.status = domain::MessageStatus::Sent;
        a1.createdAt = QDateTime::currentDateTime();
        a1.blocks.append(domain::conversation::MessageBlock(domain::BlockType::Text, domain::conversation::TextBlock{QStringLiteral("通过使用 FetchContent 精细控制，能够高效集成第三方库并避免多余构建。")}));
        initialMessages.append(a1);

        domain::conversation::Message u2;
        u2.id = QUuid::createUuid();
        u2.role = domain::MessageRole::User;
        u2.status = domain::MessageStatus::Sent;
        u2.createdAt = QDateTime::currentDateTime();
        u2.blocks.append(domain::conversation::MessageBlock(domain::BlockType::Text, domain::conversation::TextBlock{QStringLiteral("ForgeAI 的 UI 设计方案与组件选型")}));
        initialMessages.append(u2);

        domain::conversation::Message a2;
        a2.id = QUuid::createUuid();
        a2.role = domain::MessageRole::Assistant;
        a2.status = domain::MessageStatus::Sent;
        a2.createdAt = QDateTime::currentDateTime();
        a2.blocks.append(domain::conversation::MessageBlock(domain::BlockType::Text, domain::conversation::TextBlock{QStringLiteral("主界面采用 Fluent Design 2 规范，使用纯自绘 FlatExpander、Card 消息气泡与 QVBoxLayout 原生流式排版。")}));
        initialMessages.append(a2);

        domain::conversation::Message u3;
        u3.id = QUuid::createUuid();
        u3.role = domain::MessageRole::User;
        u3.status = domain::MessageStatus::Sent;
        u3.createdAt = QDateTime::currentDateTime();
        u3.blocks.append(domain::conversation::MessageBlock(domain::BlockType::Text, domain::conversation::TextBlock{QStringLiteral("ChatHeader、ChatAnchorBar 和 ChatInputBox 的落地")}));
        initialMessages.append(u3);

        domain::conversation::Message a3;
        a3.id = QUuid::createUuid();
        a3.role = domain::MessageRole::Assistant;
        a3.status = domain::MessageStatus::Sent;
        a3.createdAt = QDateTime::currentDateTime();
        a3.blocks.append(domain::conversation::MessageBlock(domain::BlockType::Text, domain::conversation::TextBlock{QStringLiteral("完成了通用组件解耦，并实现了 ChatAnchorBar 与 MessageListView 的瞬时直达定位与双向高亮同步。")}));
        initialMessages.append(a3);

        m_memoryMessages[QStringLiteral("session_1")] = initialMessages;
    }

    QList<ui::screen::chat::ChatSessionItemData> ConversationService::loadSessions() {
        QList<ui::screen::chat::ChatSessionItemData> list;
        list.append(makeSession(QStringLiteral("session_1"), QStringLiteral("ForgeAI 架构与设计讨论"), true));
        list.append(makeSession(QStringLiteral("session_2"), QStringLiteral("新对话")));
        return list;
    }

    QList<domain::conversation::Message> ConversationService::loadMessages(const QString &sessionId) {
        return m_memoryMessages.value(sessionId);
    }

    void ConversationService::saveMessages(const QString &sessionId, const QList<domain::conversation::Message> &messages) {
        m_memoryMessages[sessionId] = messages;
    }

    QString ConversationService::reuseOrCreateSession(
        QList<ui::screen::chat::ChatSessionItemData> &sessions,
        const QString &currentSessionId
    ) {
        // ① 检查现有会话列表中是否有空白且未手动命名的会话（排除当前会话）
        for (const auto &sess : sessions) {
            if (sess.title == QStringLiteral("新对话") && sess.id != currentSessionId) {
                if (m_memoryMessages.value(sess.id).isEmpty()) {
                    return sess.id;
                }
            }
        }

        // ② 若当前会话本身就是空白未改名会话，则原地复用
        if (!currentSessionId.isEmpty() && m_memoryMessages.value(currentSessionId).isEmpty()) {
            for (const auto &sess : sessions) {
                if (sess.id == currentSessionId && sess.title == QStringLiteral("新对话")) {
                    return currentSessionId;
                }
            }
        }

        // ③ 否则创建全新的会话
        static int idCounter = 100;
        const QString newId = QStringLiteral("session_%1").arg(++idCounter);
        const auto newSess = makeSession(newId, QStringLiteral("新对话"));
        sessions.prepend(newSess);
        m_memoryMessages[newId] = QList<domain::conversation::Message>();
        return newId;
    }

    QString ConversationService::deleteSession(
        QList<ui::screen::chat::ChatSessionItemData> &sessions,
        const QString &sessionId,
        const QString &currentSessionId
    ) {
        const int idx = [&]() {
            for (int i = 0; i < sessions.size(); ++i) {
                if (sessions[i].id == sessionId) return i;
            }
            return -1;
        }();

        if (idx >= 0) {
            sessions.removeAt(idx);
        }
        m_memoryMessages.remove(sessionId);

        // 如果删除的不是当前激活的会话，保持当前激活 ID
        if (sessionId != currentSessionId) {
            return currentSessionId;
        }

        // 若全部会话被删除，保底新建一个空白会话
        if (sessions.isEmpty()) {
            static int fallbackCounter = 200;
            const QString newId = QStringLiteral("session_%1").arg(++fallbackCounter);
            sessions.append(makeSession(newId, QStringLiteral("新对话")));
            m_memoryMessages[newId] = QList<domain::conversation::Message>();
            return newId;
        }

        // 邻近回退
        const int fallback = qMin(idx, sessions.size() - 1);
        return sessions[fallback].id;
    }
} // namespace services::conversation
