#include "ChatInputBox.h"

#include <QVBoxLayout>
#include <QTextEdit>
#include <QKeyEvent>
#include <QPainter>
#include <QAbstractTextDocumentLayout>
#include <FluentQt/BasicInput.h>

namespace ui::screen::chat {
    namespace {
        constexpr int kMinInputHeight = 48;
        constexpr int kMaxInputHeight = 140;
        constexpr int kMinBoxWidth = 400;
        constexpr int kMaxBoxWidth = 660;
        constexpr int kShadowMargin = 8;
        constexpr int kShadowLayers = 8;
        constexpr qreal kShadowIntensity = 0.18;
        constexpr int kShadowVerticalOffset = 2;
        constexpr qreal kBoxCornerRadius = 8.0;
    } // namespace

    ChatInputBox::ChatInputBox(QWidget *parent)
        : QWidget(parent) {
        setAttribute(Qt::WA_TranslucentBackground);
        setMinimumWidth(kMinBoxWidth);
        setMaximumWidth(kMaxBoxWidth);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setupUi();
    }

    void ChatInputBox::setupUi() {
        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(12 + kShadowMargin, 10 + kShadowMargin, 12 + kShadowMargin, 8 + kShadowMargin);
        mainLayout->setSpacing(6);

        m_textEdit = new QTextEdit(this);
        m_textEdit->setFrameShape(QFrame::NoFrame);
        m_textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_textEdit->setPlaceholderText(tr("输入消息，Enter 发送，Shift+Enter 换行..."));
        m_textEdit->setFont(themeFont(Typography::FontRole::Body).toQFont());
        m_textEdit->setFixedHeight(kMinInputHeight);
        m_textEdit->setStyleSheet(QStringLiteral("QTextEdit { background: transparent; border: none; }"));
        m_textEdit->installEventFilter(this);

        connect(m_textEdit, &QTextEdit::textChanged, this, [this]() {
            updateInputHeight();
            updateSendButtonVisual();
        });

        mainLayout->addWidget(m_textEdit);

        auto *bottomLayout = new QHBoxLayout();
        bottomLayout->setContentsMargins(0, 0, 0, 0);
        bottomLayout->setSpacing(4);

        m_attachButton = new fluent::basicinput::Button(this);
        m_attachButton->setFluentStyle(fluent::basicinput::Button::Subtle);
        m_attachButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
        m_attachButton->setIconGlyph(Typography::Icons::glyph(QStringLiteral("ic_fluent_attach_20_regular")), 13);
        m_attachButton->setFixedSize(28, 28);
        m_attachButton->setToolTip(tr("添加附件"));
        m_attachButton->setCursor(Qt::PointingHandCursor);
        connect(m_attachButton, &QPushButton::clicked, this, &ChatInputBox::attachClicked);
        bottomLayout->addWidget(m_attachButton);

        m_webSearchButton = new fluent::basicinput::Button(this);
        m_webSearchButton->setCheckable(true);
        m_webSearchButton->setFluentStyle(fluent::basicinput::Button::Subtle);
        m_webSearchButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
        m_webSearchButton->setIconGlyph(Typography::Icons::World, 13);
        m_webSearchButton->setFixedSize(28, 28);
        m_webSearchButton->setToolTip(tr("联网搜索"));
        m_webSearchButton->setCursor(Qt::PointingHandCursor);
        connect(m_webSearchButton, &QPushButton::toggled, this, &ChatInputBox::webSearchToggled);
        bottomLayout->addWidget(m_webSearchButton);

        m_deepThinkButton = new fluent::basicinput::Button(this);
        m_deepThinkButton->setCheckable(true);
        m_deepThinkButton->setFluentStyle(fluent::basicinput::Button::Subtle);
        m_deepThinkButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
        m_deepThinkButton->setIconGlyph(Typography::Icons::glyph(QStringLiteral("ic_fluent_brain_circuit_20_regular")),
                                        13);
        m_deepThinkButton->setFixedSize(28, 28);
        m_deepThinkButton->setToolTip(tr("深度思考模式"));
        m_deepThinkButton->setCursor(Qt::PointingHandCursor);
        connect(m_deepThinkButton, &QPushButton::toggled, this, &ChatInputBox::deepThinkToggled);
        bottomLayout->addWidget(m_deepThinkButton);

        bottomLayout->addStretch(1);

        m_modelButton = new fluent::basicinput::Button(this);
        m_modelButton->setFluentStyle(fluent::basicinput::Button::Subtle);
        m_modelButton->setFluentLayout(fluent::basicinput::Button::TextOnly);
        m_modelButton->setText(QStringLiteral("DeepSeek-R1  ▾"));
        m_modelButton->setFixedHeight(28);
        QFont modelFont = themeFont(Typography::FontRole::Caption).toQFont();
        m_modelButton->setFont(modelFont);
        m_modelButton->setToolTip(tr("切换当前会话模型"));
        m_modelButton->setCursor(Qt::PointingHandCursor);
        connect(m_modelButton, &QPushButton::clicked, this, &ChatInputBox::modelButtonClicked);
        bottomLayout->addWidget(m_modelButton);

        m_sendButton = new fluent::basicinput::Button(this);
        m_sendButton->setFluentStyle(fluent::basicinput::Button::Subtle);
        m_sendButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
        m_sendButton->setIconGlyph(Typography::Icons::Send, 13);
        m_sendButton->setFixedSize(28, 28);
        m_sendButton->setToolTip(tr("发送 (Enter)"));
        connect(m_sendButton, &QPushButton::clicked, this, [this]() {
            if (m_sendState == SendState::Generating) {
                emit stopRequested();
            } else if (!text().trimmed().isEmpty()) {
                const QString content = text();
                clearText();
                emit sendRequested(content);
            }
        });
        bottomLayout->addWidget(m_sendButton);

        mainLayout->addLayout(bottomLayout);

        updateSendButtonVisual();
    }

    void ChatInputBox::setSendState(SendState state) {
        if (m_sendState == state)
            return;

        m_sendState = state;
        updateSendButtonVisual();
    }

    void ChatInputBox::setModelName(const QString &name) const {
        if (m_modelButton) {
            m_modelButton->setText(QStringLiteral("%1  ▾").arg(name));
        }
    }

    QString ChatInputBox::modelName() const {
        if (!m_modelButton) return {};
        QString text = m_modelButton->text();
        text.remove(QStringLiteral("  ▾"));
        return text.trimmed();
    }

    QString ChatInputBox::text() const {
        return m_textEdit ? m_textEdit->toPlainText() : QString();
    }

    void ChatInputBox::setText(const QString &text) const {
        if (m_textEdit) {
            m_textEdit->setPlainText(text);
        }
    }

    void ChatInputBox::clearText() const {
        if (m_textEdit) {
            m_textEdit->clear();
        }
    }

    void ChatInputBox::onThemeUpdated() {
        if (m_textEdit) {
            m_textEdit->setFont(themeFont(Typography::FontRole::Body).toQFont());
        }
        update();
    }

    void ChatInputBox::updateInputHeight() const {
        if (!m_textEdit) return;

        const int docHeight = qRound(m_textEdit->document()->documentLayout()->documentSize().height());
        const int targetHeight = qBound(kMinInputHeight, docHeight + 8, kMaxInputHeight);
        if (m_textEdit->height() != targetHeight) {
            m_textEdit->setFixedHeight(targetHeight);
        }
    }

    void ChatInputBox::updateSendButtonVisual() {
        if (!m_sendButton) return;

        if (m_sendState == SendState::Generating) {
            m_sendButton->setFluentStyle(fluent::basicinput::Button::Subtle);
            m_sendButton->setIconGlyph(Typography::Icons::Stop, 13);
            m_sendButton->setToolTip(tr("停止生成"));
            m_sendButton->setEnabled(true);
            m_sendButton->setCursor(Qt::PointingHandCursor);
        } else if (!text().trimmed().isEmpty()) {
            m_sendState = SendState::Ready;
            m_sendButton->setFluentStyle(fluent::basicinput::Button::Accent);
            m_sendButton->setIconGlyph(Typography::Icons::Send, 13);
            m_sendButton->setToolTip(tr("发送 (Enter)"));
            m_sendButton->setEnabled(true);
            m_sendButton->setCursor(Qt::PointingHandCursor);
        } else {
            m_sendState = SendState::Idle;
            m_sendButton->setFluentStyle(fluent::basicinput::Button::Subtle);
            m_sendButton->setIconGlyph(Typography::Icons::Send, 13);
            m_sendButton->setToolTip(tr("输入内容后发送"));
            m_sendButton->setEnabled(false);
            m_sendButton->setCursor(Qt::ArrowCursor);
        }
    }

    bool ChatInputBox::eventFilter(QObject *watched, QEvent *event) {
        if (watched == m_textEdit && event->type() == QEvent::KeyPress) {
            auto *keyEvent = dynamic_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                if (keyEvent->modifiers() & Qt::ShiftModifier) {
                    return false; // Shift+Enter 正常换行
                }

                // Enter 触发发送或停止
                if (m_sendState == SendState::Generating) {
                    emit stopRequested();
                    return true;
                }

                if (!text().trimmed().isEmpty()) {
                    const QString content = text();
                    clearText();
                    emit sendRequested(content);
                    return true;
                }
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void ChatInputBox::paintEvent(QPaintEvent *event) {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const auto &colors = themeColorsRef();
        const QRectF boxRect = QRectF(rect()).adjusted(
            kShadowMargin + 0.5,
            kShadowMargin + 0.5,
            -kShadowMargin - 0.5,
            -kShadowMargin - 0.5
        );

        const auto shadow = themeShadow(Elevation::Low);
        painter.setPen(Qt::NoPen);
        for (int i = 0; i < kShadowLayers; ++i) {
            const qreal ratio = 1.0 - static_cast<qreal>(i) / kShadowLayers;
            const qreal smoothRatio = ratio * ratio;
            QColor layerColor = shadow.color;
            layerColor.setAlphaF(static_cast<float>(shadow.opacity * smoothRatio * kShadowIntensity));
            painter.setBrush(layerColor);
            painter.drawRoundedRect(
                boxRect.adjusted(-i, -i, i, i).translated(0, kShadowVerticalOffset),
                kBoxCornerRadius + i,
                kBoxCornerRadius + i
            );
        }

        painter.setPen(QPen(colors.strokeDefault, 1.0));
        painter.setBrush(colors.bgLayer);
        painter.drawRoundedRect(boxRect, kBoxCornerRadius, kBoxCornerRadius);
    }
} // namespace ui::screen::chat
