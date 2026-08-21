#include "ChatSessionListModel.h"

#include <algorithm>

namespace ui::screen::chat {
    ChatSessionListModel::ChatSessionListModel(QObject *parent)
        : QAbstractListModel(parent) {
    }

    int ChatSessionListModel::rowCount(const QModelIndex &parent) const {
        if (parent.isValid())
            return 0;
        return static_cast<int>(m_sessions.size());
    }

    QVariant ChatSessionListModel::data(const QModelIndex &index, int role) const {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_sessions.size())
            return {};

        const auto &item = m_sessions.at(index.row());
        switch (role) {
            case Qt::DisplayRole:
            case TitleRole:
                return item.title;
            case IdRole:
                return item.id;
            case IsPinnedRole:
                return item.isPinned;
            case TimestampRole:
                return item.timestamp;
            default:
                return {};
        }
    }

    QHash<int, QByteArray> ChatSessionListModel::roleNames() const {
        return {
            {IdRole, "id"},
            {TitleRole, "title"},
            {IsPinnedRole, "isPinned"},
            {TimestampRole, "timestamp"}
        };
    }

    void ChatSessionListModel::setSessions(const QList<ChatSessionItemData> &sessions) {
        beginResetModel();
        m_sessions = sessions;
        sortSessions();
        endResetModel();
    }

    void ChatSessionListModel::addSession(const ChatSessionItemData &session) {
        for (int i = 0; i < m_sessions.size(); ++i) {
            if (m_sessions[i].id == session.id)
                return;
        }

        beginResetModel();
        m_sessions.prepend(session);
        sortSessions();
        endResetModel();
    }

    void ChatSessionListModel::removeSession(const QString &id) {
        const int idx = indexOf(id);
        if (idx < 0)
            return;

        beginRemoveRows(QModelIndex(), idx, idx);
        m_sessions.removeAt(idx);
        endRemoveRows();
    }

    void ChatSessionListModel::setSessionPinned(const QString &id, bool pinned) {
        const int idx = indexOf(id);
        if (idx < 0)
            return;

        if (m_sessions[idx].isPinned == pinned)
            return;

        beginResetModel();
        m_sessions[idx].isPinned = pinned;
        sortSessions();
        endResetModel();
    }

    void ChatSessionListModel::setSessionTitle(const QString &id, const QString &title) {
        const int idx = indexOf(id);
        if (idx < 0)
            return;

        m_sessions[idx].title = title;
        const QModelIndex modelIdx = index(idx, 0);
        emit dataChanged(modelIdx, modelIdx, {Qt::DisplayRole, TitleRole});
    }

    void ChatSessionListModel::clearSessions() {
        beginResetModel();
        m_sessions.clear();
        endResetModel();
    }

    ChatSessionItemData ChatSessionListModel::sessionAt(int row) const {
        if (row >= 0 && row < m_sessions.size())
            return m_sessions.at(row);
        return {};
    }

    int ChatSessionListModel::indexOf(const QString &id) const {
        for (int i = 0; i < m_sessions.size(); ++i) {
            if (m_sessions[i].id == id)
                return i;
        }
        return -1;
    }

    QString ChatSessionListModel::idAt(int row) const {
        if (row >= 0 && row < m_sessions.size())
            return m_sessions.at(row).id;
        return {};
    }

    void ChatSessionListModel::sortSessions() {
        // 稳定排序：置顶在前，时间戳靠后排前（越新越靠前）
        std::stable_sort(m_sessions.begin(), m_sessions.end(),
                         [](const ChatSessionItemData &a, const ChatSessionItemData &b) {
                             if (a.isPinned != b.isPinned)
                                 return a.isPinned > b.isPinned;
                             return a.timestamp > b.timestamp;
                         });
    }
} // namespace ui::screen::chat
