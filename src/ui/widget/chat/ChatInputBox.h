#pragma once

#include <QWidget>
#include <QString>
#include <FluentQt/Foundation.h>
#include <FluentQt/Design.h>

class QTextEdit;

namespace fluent::basicinput {
    class Button;
}

namespace ui::widget::chat {
    /**
     * @brief 底部一体化智能输入控制台（自适应输入框 + 底部左侧工具栏 + 底部右侧模型切换与发送状态机）
     */
    class ChatInputBox : public QWidget, public fluent::FluentElement {
        Q_OBJECT

    public:
        enum class SendState {
            Idle, // 无输入内容
            Ready, // 已输入内容，准备发送
            Generating // 模型正在流式生成中，可点击停止
        };

        Q_ENUM(SendState)

        explicit ChatInputBox(QWidget *parent = nullptr);

        ~ChatInputBox() override = default;

        void setSendState(SendState state);

        SendState sendState() const { return m_sendState; }

        void setModelName(const QString &name) const;

        QString modelName() const;

        QString text() const;

        void setText(const QString &text) const;

        void clearText() const;

        void onThemeUpdated() override;

    Q_SIGNALS:
        void sendRequested(const QString &text);

        void stopRequested();

        void modelButtonClicked();

        void attachClicked();

        void webSearchToggled(bool enabled);

        void deepThinkToggled(bool enabled);

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;

        void paintEvent(QPaintEvent *event) override;

    private:
        void setupUi();

        void updateInputHeight() const;

        void updateSendButtonVisual();

        SendState m_sendState = SendState::Idle;
        QTextEdit *m_textEdit = nullptr;
        fluent::basicinput::Button *m_attachButton = nullptr;
        fluent::basicinput::Button *m_webSearchButton = nullptr;
        fluent::basicinput::Button *m_deepThinkButton = nullptr;
        fluent::basicinput::Button *m_modelButton = nullptr;
        fluent::basicinput::Button *m_sendButton = nullptr;
    };
} // namespace ui::widget::chat
