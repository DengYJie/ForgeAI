#pragma once

#include <QWidget>
#include <QPointer>
#include <FluentQt/Foundation.h>
#include <FluentQt/Design.h>

namespace fluent::basicinput {
    class Button;
}

namespace fluent::collections {
    class ListView;
}

namespace ui::screen::chat {
    class ChatSessionListModel;
    class ChatSessionDelegate;

    /**
     * @brief 会话侧边栏组件，基于 FluentQt Model/View 架构与原生 Button 实现
     */
    class ChatSidebar : public QWidget, public fluent::FluentElement {
        Q_OBJECT

    public:
        explicit ChatSidebar(QWidget *parent = nullptr);

        ~ChatSidebar() override = default;

        /**
         * @brief 添加一个会话项
         * @param id 会话唯一标识
         * @param title 会话标题
         * @param isPinned 是否置顶
         */
        void addSession(const QString &id, const QString &title, bool isPinned = false);

        /**
         * @brief 选中指定会话
         */
        void selectSession(const QString &id);

        /**
         * @brief 移除指定会话
         */
        void removeSession(const QString &id);

        /**
         * @brief 设置会话置顶状态
         */
        void setSessionPinned(const QString &id, bool pinned);

        /**
         * @brief 设置会话标题
         */
        void setSessionTitle(const QString &id, const QString &title);

        /**
         * @brief 清空所有会话
         */
        void clearSessions();

        /**
         * @brief 获取当前选中的会话 ID
         */
        QString currentSelectedSessionId() const;

        void onThemeUpdated() override;

    Q_SIGNALS:
        /**
         * @brief 点击“新对话”按钮时触发
         */
        void newChatRequested();

        /**
         * @brief 点击过滤/菜单按钮时触发
         */
        void filterRequested();

        /**
         * @brief 切换选中会话时触发
         */
        void sessionSelected(const QString &id);

        /**
         * @brief 删除会话时触发
         */
        void sessionDeleted(const QString &id);

        /**
         * @brief 置顶状态变更时触发
         */
        void sessionPinToggled(const QString &id, bool pinned);

    private:
        void setupUi();

        QWidget *m_headerWidget = nullptr;
        fluent::basicinput::Button *m_newChatButton = nullptr;
        fluent::basicinput::Button *m_filterButton = nullptr;

        fluent::collections::ListView *m_listView = nullptr;
        ChatSessionListModel *m_model = nullptr;
        ChatSessionDelegate *m_delegate = nullptr;
    };
} // namespace ui::screen::chat
