#include "ConversationService.h"
#include "domain/repository/IConversationRepository.h"
#include "domain/repository/IMessageTranscriptRepository.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfoList>
#include <QSet>
#include <QUuid>
#include <QDebug>

namespace services::conversation {
    namespace {
        ui::screen::chat::ChatSessionItemData makeSession(const QString &id, const QString &title, bool isPinned = false, bool isArchived = false) {
            ui::screen::chat::ChatSessionItemData s;
            s.id = id;
            s.title = title;
            s.isPinned = isPinned;
            s.isArchived = isArchived;
            s.timestamp = QDateTime::currentMSecsSinceEpoch();
            return s;
        }
    } // namespace

    ConversationService::ConversationService(
        domain::repository::IConversationRepository *conversationRepo,
        domain::repository::IMessageTranscriptRepository *transcriptRepo,
        QObject *parent
    ) : IConversationService(parent), m_conversationRepo(conversationRepo), m_transcriptRepo(transcriptRepo) {
    }

    QList<ui::screen::chat::ChatSessionItemData> ConversationService::loadSessions(const std::optional<QUuid>& projectId) {
        QList<ui::screen::chat::ChatSessionItemData> list;
        if (m_conversationRepo) {
            const auto allConvs = m_conversationRepo->getAllConversations();
            for (const auto& conversation : allConvs) {
                // 严格进行项目归属隔离过滤：
                // 若指定了 projectId，仅返回归属于该项目的会话；
                // 若未指定 projectId（即常规 Chat 页面），仅返回无 projectId 的普通全局对话！
                if (projectId.has_value()) {
                    if (!conversation.projectId.has_value() || *conversation.projectId != *projectId) {
                        continue;
                    }
                } else {
                    if (conversation.projectId.has_value()) {
                        continue;
                    }
                }

                auto session = makeSession(conversation.id.toString(), conversation.title, conversation.isPinned, conversation.isArchived);
                session.projectId = conversation.projectId;
                session.timestamp = conversation.updatedAt.toMSecsSinceEpoch();
                list.append(std::move(session));
            }

            qInfo().noquote() << QStringLiteral("[ConversationService] loadSessions -> filter projectId: %1, total in repo: %2, filtered: %3")
                .arg(projectId.has_value() ? projectId->toString() : QStringLiteral("None (Global Chat)"),
                     QString::number(allConvs.size()), QString::number(list.size()));

            // 仅对无 projectId 的普通对话执行未持久化转录文件的容灾恢复
            if (m_transcriptRepo && !projectId.has_value()) {
                QSet<QString> knownIds;
                for (const auto& conversation : allConvs) {
                    knownIds.insert(conversation.id.toString());
                    knownIds.insert(conversation.id.toString(QUuid::WithoutBraces));
                }
                const QDir sessionDir(QDir::homePath() + QStringLiteral("/.forgeai/sessions"));
                for (const QFileInfo& file : sessionDir.entryInfoList({QStringLiteral("*.jsonl")}, QDir::Files)) {
                    const QString baseName = file.completeBaseName();
                    const QUuid id = QUuid::fromString(baseName);
                    if (id.isNull() || knownIds.contains(id.toString()) || knownIds.contains(id.toString(QUuid::WithoutBraces))) continue;
                    const auto messages = m_transcriptRepo->getMessagesByConversationId(id);
                    QString title = QStringLiteral("新对话");
                    for (const auto& message : messages) {
                        if (message.role != domain::MessageRole::User) continue;
                        for (const auto& block : message.blocks) if (block.isText()) {
                            const QString text = std::get<domain::conversation::TextBlock>(block.payload).text.trimmed();
                            if (!text.isEmpty()) { title = text.left(18) + (text.size() > 18 ? QStringLiteral("...") : QString()); break; }
                        }
                        if (title != QStringLiteral("新对话")) break;
                    }
                    domain::conversation::Conversation conversation;
                    conversation.id = id; conversation.title = title;
                    conversation.createdAt = QDateTime::fromMSecsSinceEpoch(file.birthTime().toMSecsSinceEpoch());
                    conversation.updatedAt = QDateTime::fromMSecsSinceEpoch(file.lastModified().toMSecsSinceEpoch());
                    m_conversationRepo->saveConversation(conversation);
                    list.append(makeSession(id.toString(), title));
                    qInfo().noquote() << QStringLiteral("[ConversationService] Recovered untracked transcript: %1 (%2)").arg(id.toString(), title);
                }
            }
        }
        return list;
    }

    QList<domain::conversation::Message> ConversationService::loadMessages(const QString &sessionId) {
        if (m_memoryMessages.contains(sessionId)) return m_memoryMessages.value(sessionId);
        if (m_transcriptRepo) {
            const auto messages = m_transcriptRepo->getMessagesByConversationId(QUuid(sessionId));
            m_memoryMessages.insert(sessionId, messages);
            return messages;
        }
        return m_memoryMessages.value(sessionId);
    }

    void ConversationService::saveMessages(const QString &sessionId, const QList<domain::conversation::Message> &messages) {
        m_memoryMessages[sessionId] = messages;
        const QUuid id = QUuid::fromString(sessionId);
        if (m_transcriptRepo) {
            m_transcriptRepo->deleteTranscript(id);
            for (const auto& message : messages) m_transcriptRepo->appendMessage(id, message);
        }
        if (m_conversationRepo) {
            const auto conversation = m_conversationRepo->getConversation(id);
            auto updated = conversation.value_or(domain::conversation::Conversation{});
            updated.id = id;
            if (conversation.has_value()) {
                updated.projectId = conversation->projectId;
                updated.agentId = conversation->agentId;
                if (!conversation->title.isEmpty()) updated.title = conversation->title;
            }
            if (updated.title.isEmpty()) updated.title = QStringLiteral("新对话");
            if (!updated.createdAt.isValid()) updated.createdAt = QDateTime::currentDateTime();
            updated.updatedAt = QDateTime::currentDateTime();
            m_conversationRepo->saveConversation(updated);

            qInfo().noquote() << QStringLiteral("[ConversationService] saveMessages -> sessionId: %1, messages: %2, projectId: %3, title: %4")
                .arg(sessionId, QString::number(messages.size()),
                     updated.projectId.has_value() ? updated.projectId->toString() : QStringLiteral("None"), updated.title);
        }
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
        const QString newId = QUuid::createUuid().toString();
        const auto newSess = makeSession(newId, QStringLiteral("新对话"));
        sessions.prepend(newSess);
        m_memoryMessages[newId] = QList<domain::conversation::Message>();
        if (m_conversationRepo) {
            domain::conversation::Conversation conversation;
            conversation.id = QUuid(newId);
            conversation.title = newSess.title;
            conversation.createdAt = QDateTime::currentDateTime();
            conversation.updatedAt = conversation.createdAt;
            m_conversationRepo->saveConversation(conversation);
        }
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
        if (m_conversationRepo) m_conversationRepo->deleteConversation(QUuid(sessionId));
        if (m_transcriptRepo) m_transcriptRepo->deleteTranscript(QUuid(sessionId));

        // 如果删除的不是当前激活的会话，保持当前激活 ID
        if (sessionId != currentSessionId) {
            return currentSessionId;
        }

        // 若全部会话被删除，保底新建一个空白会话
        if (sessions.isEmpty()) {
            const QString newId = QUuid::createUuid().toString();
            sessions.append(makeSession(newId, QStringLiteral("新对话")));
            m_memoryMessages[newId] = QList<domain::conversation::Message>();
            return newId;
        }

        // 邻近回退
        const int fallback = qMin(idx, sessions.size() - 1);
        return sessions[fallback].id;
    }

    void ConversationService::setSessionPinned(QList<ui::screen::chat::ChatSessionItemData>& sessions,
                                               const QString& sessionId, bool pinned) {
        for (auto& session : sessions) {
            if (session.id != sessionId) continue;
            session.isPinned = pinned;
            if (m_conversationRepo) {
                const auto conversation = m_conversationRepo->getConversation(QUuid(sessionId));
                if (conversation) {
                    auto updated = *conversation;
                    updated.isPinned = pinned;
                    m_conversationRepo->saveConversation(updated);
                }
            }
            break;
        }
    }

    void ConversationService::setSessionArchived(QList<ui::screen::chat::ChatSessionItemData>& sessions,
                                                 const QString& sessionId, bool archived) {
        for (auto& session : sessions) {
            if (session.id != sessionId) continue;
            session.isArchived = archived;
            if (archived) session.isPinned = false;
            if (m_conversationRepo) {
                const auto conversation = m_conversationRepo->getConversation(QUuid(sessionId));
                if (conversation) {
                    auto updated = *conversation;
                    updated.isArchived = archived;
                    if (archived) updated.isPinned = false;
                    updated.updatedAt = QDateTime::currentDateTime();
                    m_conversationRepo->saveConversation(updated);
                }
            }
            break;
        }
    }

    void ConversationService::setSessionTitle(QList<ui::screen::chat::ChatSessionItemData>& sessions,
                                              const QString& sessionId, const QString& title) {
        for (auto& session : sessions) {
            if (session.id != sessionId) continue;
            session.title = title;
            session.timestamp = QDateTime::currentMSecsSinceEpoch();
            if (m_conversationRepo) {
                const auto conversation = m_conversationRepo->getConversation(QUuid(sessionId));
                if (conversation) {
                    auto updated = *conversation;
                    updated.title = title;
                    updated.updatedAt = QDateTime::currentDateTime();
                    m_conversationRepo->saveConversation(updated);
                }
            }
            break;
        }
    }
} // namespace services::conversation
