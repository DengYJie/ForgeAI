#pragma once

#include <QString>
#include <QUrl>
#include <QUuid>
#include <QWidget>

#include "domain/conversation/Message.h"
#include <FluentQt/BasicInput.h>
#include <FluentQt/Design.h>
#include <FluentQt/Foundation.h>
#include <FluentQt/Layout.h>
#include <FluentQt/StatusInfo.h>
#include <FluentQt/TextFields.h>

class QHBoxLayout;
class QVBoxLayout;

namespace ui::widget {
    class MarkdownView;
}

namespace ui::widget::message {

    namespace blocks {
        class ThinkingBlockWidget;
        class AbstractToolBlockWidget;
        class ErrorBlockWidget;
    }

    /**
     * @brief 现代化消息卡片控件 (支持用户气泡、AI富文本排版、思考/工具过程折叠与操作栏)
     */
    class MessageCardWidget : public QWidget, public fluent::FluentElement
    {
        Q_OBJECT

    public:
        explicit MessageCardWidget(QWidget* parent = nullptr);
        explicit MessageCardWidget(const domain::conversation::Message& message, QWidget* parent = nullptr);
        ~MessageCardWidget() override;

        void setMessage(const domain::conversation::Message& message);
        const domain::conversation::Message& message() const { return m_message; }
        QUuid messageId() const { return m_message.id; }
        domain::MessageRole role() const { return m_message.role; }

        // 数据驱动同步
        void syncMessage(const domain::conversation::Message& message);
        // 虚拟列表复用到另一条消息前清理思考、工具等瞬态子控件。
        void resetForReuse();

        // 插入外部错误诊断块
        void appendError(const QString& summary, const QString& details);

        void setSenderName(const QString& name);
        QString senderName() const { return m_senderName; }

        // 头像与头部显示开关
        void setAvatarVisible(bool visible);
        bool isAvatarVisible() const { return m_avatarVisible; }

        void setHeaderVisible(bool visible);
        bool isHeaderVisible() const { return m_headerVisible; }

        bool hasHeightForWidth() const override;
        int heightForWidth(int width) const override;
        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

        void onThemeUpdated() override;

        class ProcessGroupWidget* processGroup();

    signals:
        void linkActivated(const QUrl& url);
        void contentHeightChanged();

    protected:
        void resizeEvent(QResizeEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        void setupUi();
        void updateVisuals();
        void updateActionBarVisibility();

        domain::conversation::Message m_message;
        QString m_senderName;
        bool m_avatarVisible = false;
        bool m_headerVisible = true;
        domain::MessageRole m_currentVisualRole = domain::MessageRole::System;
        bool m_currentVisualAvatarVisible = false;
        bool m_currentVisualHeaderVisible = false;
        bool m_visualsConstructed = false;

        QVBoxLayout* m_mainLayout = nullptr;

        // 助手模式顶部 Header
        QWidget* m_headerWidget = nullptr;
        QHBoxLayout* m_headerLayout = nullptr;
        fluent::status_info::Avatar* m_avatar = nullptr;
        fluent::textfields::Label* m_senderLabel = nullptr;
        fluent::textfields::Label* m_timeLabel = nullptr;

        // 内容区（用户气泡 Card / 助手文档流）
        QWidget* m_contentRow = nullptr;
        QHBoxLayout* m_contentRowLayout = nullptr;
        fluent::layout::Card* m_userBubbleCard = nullptr;
        QVBoxLayout* m_bubbleLayout = nullptr;
        ui::widget::MarkdownView* m_markdownView = nullptr;
        ProcessGroupWidget* m_processGroupWidget = nullptr;

        // 操作栏
        QWidget* m_actionBar = nullptr;
        QHBoxLayout* m_actionLayout = nullptr;
        fluent::basicinput::Button* m_copyButton = nullptr;

        // 状态记录，用于增量 Diff
        QList<ui::widget::message::blocks::ThinkingBlockWidget*> m_thoughtWidgets;
        QMap<QString, ui::widget::message::blocks::AbstractToolBlockWidget*> m_toolWidgets;
        ui::widget::message::blocks::ErrorBlockWidget* m_errorWidget = nullptr;
    };

} // namespace ui::widget::message
