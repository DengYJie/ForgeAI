#pragma once

#include <QMap>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QUuid>
#include <QVariantAnimation>

#include <QVBoxLayout>
#include "domain/conversation/Message.h"
#include <FluentQt/Scrolling.h>

namespace ui::widget::message {

    class MessageCardWidget;

    /**
     * @brief 聊天消息列表控件
     * 负责管理多个 MessageCardWidget 的增量 Diff 刷新与平滑滚底逻辑。
     * 支持通过 setCustomScrollBar 注入外部自定义滚动条以实现特殊布局排版。
     */
    class MessageListView : public fluent::scrolling::ScrollView {
        Q_OBJECT
    public:
        explicit MessageListView(QWidget* parent = nullptr);
        ~MessageListView() override;

        // 唯一的外部数据输入口
        void syncMessages(const QList<domain::conversation::Message>& messages);

        // 清空列表
        void clear();

        // 注入外部自定义滚动条
        void setCustomScrollBar(QScrollBar* scrollBar);

        // 手动平滑滚到底部
        void scrollToBottom();

        // 瞬间直达定位到指定消息
        void scrollToMessage(const QUuid& id);
        void scrollToMessage(const QString& idString);

        // 全局头像与头部显示开关控制
        void setAvatarVisible(bool visible);
        bool isAvatarVisible() const { return m_avatarVisible; }

        void setHeaderVisible(bool visible);
        bool isHeaderVisible() const { return m_headerVisible; }

    Q_SIGNALS:
        // 视口滚动时触发，通知当前位于视口顶部的消息ID
        void topVisibleMessageChanged(const QUuid& id);

    protected:
        void resizeEvent(QResizeEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;

        void onThemeUpdated() override;

    private slots:
        void executeFollowBottom();
        void scheduleFollowBottom();
        void onCardHeightChanged();
        void checkTopVisibleMessage();

    private:
        void setupUi();
        void bindScrollBarSignals(QScrollBar* bar);
        void updateScrollGeometry();

        QWidget* m_container = nullptr;
        QVBoxLayout* m_layout = nullptr;

        // UUID 到气泡 Widget 的映射表 (用于 Diff 比对)
        QMap<QUuid, MessageCardWidget*> m_cardMap;
        // 记录各卡片上一次的高度（用于视口滚动锚定）
        QMap<MessageCardWidget*, int> m_cardLastHeights;

        // 平滑滚动相关
        QScrollBar* m_customScrollBar = nullptr;
        QTimer* m_followTimer = nullptr;
        QTimer* m_visibleCheckTimer = nullptr;
        QVariantAnimation* m_scrollAnimation = nullptr;
        bool m_autoScrollToBottom = true;
        QUuid m_lastTopVisibleId;

        bool m_avatarVisible = true;
        bool m_headerVisible = true;
    };

} // namespace ui::widget::message
