#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

namespace ui::screen::chat {
    struct ChatSessionItemData {
        QString id;
        QString title;
        bool isPinned = false;
        qint64 timestamp = 0;

        bool operator==(const ChatSessionItemData &other) const {
            return id == other.id && title == other.title && isPinned == other.isPinned && timestamp == other.timestamp;
        }
    };

    class ChatSessionListModel : public QAbstractListModel {
        Q_OBJECT

    public:
        enum ChatSessionRoles {
            IdRole = Qt::UserRole + 1,
            TitleRole,
            IsPinnedRole,
            TimestampRole
        };

        Q_ENUM(ChatSessionRoles)

        explicit ChatSessionListModel(QObject *parent = nullptr);

        ~ChatSessionListModel() override = default;

        int rowCount(const QModelIndex &parent = QModelIndex()) const override;

        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

        QHash<int, QByteArray> roleNames() const override;

        void setSessions(const QList<ChatSessionItemData> &sessions);

        void addSession(const ChatSessionItemData &session);

        void removeSession(const QString &id);

        void setSessionPinned(const QString &id, bool pinned);

        void setSessionTitle(const QString &id, const QString &title);

        void clearSessions();

        ChatSessionItemData sessionAt(int row) const;

        int indexOf(const QString &id) const;

        QString idAt(int row) const;

        int count() const { return m_sessions.size(); }

    private:
        void sortSessions();

        QList<ChatSessionItemData> m_sessions;
    };
} // namespace ui::screen::chat
