#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include "domain/conversation/Message.h"
#include "ui/screen/chat/ChatSessionListModel.h"

namespace domain::service {
    /**
     * @brief 会话与历史消息生命周期管理服务接口
     */
    class IConversationService : public QObject {
        Q_OBJECT

    public:
        using QObject::QObject;
        ~IConversationService() override = default;

        /**
         * @brief 加载初始化会话列表
         */
        virtual QList<ui::screen::chat::ChatSessionItemData> loadSessions() = 0;

        /**
         * @brief 加载指定会话的消息历史
         */
        virtual QList<domain::conversation::Message> loadMessages(const QString &sessionId) = 0;

        /**
         * @brief 存储指定会话的消息历史
         */
        virtual void saveMessages(const QString &sessionId, const QList<domain::conversation::Message> &messages) = 0;

        /**
         * @brief 复用已有的空白未命名会话，若无则新建
         * @param sessions 会话列表引用（会被就地更新）
         * @param currentSessionId 当前激活的会话 ID
         * @return 激活的目标会话 ID
         */
        virtual QString reuseOrCreateSession(
            QList<ui::screen::chat::ChatSessionItemData> &sessions,
            const QString &currentSessionId
        ) = 0;

        /**
         * @brief 删除指定会话，并在当前会话被删除时自动计算邻近回退会话
         * @param sessions 会话列表引用（会被就地更新）
         * @param sessionId 待删除的会话 ID
         * @param currentSessionId 当前激活的会话 ID
         * @return 删除后应激活的会话 ID
         */
        virtual QString deleteSession(
            QList<ui::screen::chat::ChatSessionItemData> &sessions,
            const QString &sessionId,
            const QString &currentSessionId
        ) = 0;
    };
} // namespace domain::service
