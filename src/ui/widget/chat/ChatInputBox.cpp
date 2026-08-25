#include "ChatInputBox.h"

#include <QVBoxLayout>
#include <QTextEdit>
#include <QKeyEvent>
#include <QPainter>
#include <QAbstractTextDocumentLayout>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QFontMetrics>
#include <FluentQt/BasicInput.h>

namespace ui::widget::chat {
    namespace {
        constexpr int kMinInputHeight = 48;
        constexpr int kMaxInputHeight = 140;
        constexpr int kMinBoxWidth = 200;
        constexpr int kMaxBoxWidth = 1000;
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

    QSize ChatInputBox::sizeHint() const {
        return QSize(kMaxBoxWidth, kMinInputHeight + 48 + 2 * kShadowMargin);
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
        m_modelButton->setFixedHeight(28);
        QFont modelFont = themeFont(Typography::FontRole::Caption).toQFont();
        m_modelButton->setFont(modelFont);
        m_modelButton->setToolTip(tr("切换当前会话模型"));
        m_modelButton->setCursor(Qt::PointingHandCursor);
        connect(m_modelButton, &QPushButton::clicked, this, &ChatInputBox::modelButtonClicked);
        bottomLayout->addWidget(m_modelButton);

        m_modelExpandAnim = new QVariantAnimation(this);
        m_modelExpandAnim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_modelExpandAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
            if (m_modelButton) {
                m_modelButton->setFixedWidth(val.toInt());
            }
        });

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

        updateModelButtonDisplay();
        updateSendButtonVisual();
    }

    void ChatInputBox::setSendState(SendState state) {
        if (m_sendState == state)
            return;

        m_sendState = state;
        updateSendButtonVisual();
    }

    void ChatInputBox::setModelName(const QString &name) {
        setModelPresentation(name, {});
    }

    void ChatInputBox::setModelPresentation(const QString& name, const QString& reasoningEffort) {
        m_currentModelName = name;
        m_currentReasoningEffort = reasoningEffort;
        updateModelButtonDisplay();
    }

    int ChatInputBox::calculateCompactModelButtonWidth() const {
        if (m_isIconOnlyMode) return 28;
        QFont font = m_modelButton ? m_modelButton->font() : themeFont(Typography::FontRole::Caption).toQFont();
        QFontMetrics fm(font);
        const QString effortLabel = m_currentReasoningEffort.isEmpty() ? QString() : QStringLiteral("  %1").arg(m_currentReasoningEffort);
        const QString labelText = m_currentModelName.isEmpty() ? tr("选择模型  ▾") : QStringLiteral("%1%2  ▾").arg(m_currentModelName, effortLabel);
        const int textW = fm.horizontalAdvance(labelText);
        return qMax(48, textW + 18);
    }

    void ChatInputBox::updateModelButtonDisplay() {
        if (!m_modelButton) return;

        if (m_isIconOnlyMode) {
            m_modelButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
            m_modelButton->setIconGlyph(Typography::Icons::glyph(QStringLiteral("ic_fluent_brain_circuit_20_regular")), 13);
            m_modelButton->setFixedSize(28, 28);
            const QString effortLabel = m_currentReasoningEffort.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(m_currentReasoningEffort);
            const QString displayName = m_currentModelName.isEmpty() ? tr("选择模型") : m_currentModelName;
            m_modelButton->setToolTip(tr("当前模型：%1%2\n点击切换模型").arg(displayName, effortLabel));
        } else {
            m_modelButton->setFluentLayout(fluent::basicinput::Button::TextOnly);
            const QString effortLabel = m_currentReasoningEffort.isEmpty() ? QString() : QStringLiteral("  %1").arg(m_currentReasoningEffort);
            const QString labelText = m_currentModelName.isEmpty() ? tr("选择模型  ▾") : QStringLiteral("%1%2  ▾").arg(m_currentModelName, effortLabel);
            m_modelButton->setText(labelText);
            m_modelButton->setFixedHeight(28);
            m_modelButton->setToolTip(tr("切换当前会话模型"));
            const int compactW = calculateCompactModelButtonWidth();
            if (!m_isModelMenuOpen && (!m_modelExpandAnim || m_modelExpandAnim->state() != QAbstractAnimation::Running)) {
                m_modelButton->setFixedWidth(compactW);
            }
        }
    }

    void ChatInputBox::notifyModelMenuOpened() {
        m_isModelMenuOpen = true;
        // 展开动画态只在宽度足够时（宽度 >= 360 且非 IconOnly）
        if (!m_isIconOnlyMode && width() >= 360 && m_modelButton && m_modelExpandAnim) {
            m_modelExpandAnim->stop();
            m_modelExpandAnim->setDuration(themeAnimation().fast);
            m_modelExpandAnim->setStartValue(m_modelButton->width());
            m_modelExpandAnim->setEndValue(168);
            m_modelExpandAnim->start();
        }
    }

    void ChatInputBox::notifyModelMenuClosed() {
        m_isModelMenuOpen = false;
        if (!m_isIconOnlyMode && m_modelButton && m_modelExpandAnim && m_modelButton->width() != calculateCompactModelButtonWidth()) {
            const int targetW = calculateCompactModelButtonWidth();
            m_modelExpandAnim->stop();
            m_modelExpandAnim->setDuration(themeAnimation().fast);
            m_modelExpandAnim->setStartValue(m_modelButton->width());
            m_modelExpandAnim->setEndValue(targetW);
            connect(m_modelExpandAnim, &QVariantAnimation::finished, this, [this]() {
                m_modelExpandAnim->disconnect(SIGNAL(finished()));
                updateModelButtonDisplay();
            });
            m_modelExpandAnim->start();
        } else {
            updateModelButtonDisplay();
        }
    }

    void ChatInputBox::setToolAvailability(bool attachments, bool webSearch, bool deepThinking) const {
        m_attachButton->setEnabled(attachments);
        m_webSearchButton->setEnabled(webSearch);
        m_deepThinkButton->setEnabled(deepThinking);
        if (!webSearch) m_webSearchButton->setChecked(false);
        if (!deepThinking) m_deepThinkButton->setChecked(false);
    }

    QString ChatInputBox::modelName() const {
        if (!m_modelButton) return {};
        if (!m_currentModelName.isEmpty()) return m_currentModelName;
        QString text = m_modelButton->text();
        text.remove(QStringLiteral("  ▾"));
        return text.trimmed();
    }

    QWidget* ChatInputBox::modelAnchor() const { return m_modelButton; }

    QString ChatInputBox::text() const {
        return m_textEdit ? m_textEdit->toPlainText() : QString();
    }

    bool ChatInputBox::webSearchEnabled() const { return m_webSearchButton && m_webSearchButton->isChecked(); }

    bool ChatInputBox::deepThinkingEnabled() const { return m_deepThinkButton && m_deepThinkButton->isChecked(); }

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
        if (m_modelButton) {
            m_modelButton->setFont(themeFont(Typography::FontRole::Caption).toQFont());
            updateModelButtonDisplay();
        }
        update();
    }

    void ChatInputBox::resizeEvent(QResizeEvent *event) {
        QWidget::resizeEvent(event);
        const bool shouldBeIconOnly = (width() < 360);
        if (shouldBeIconOnly != m_isIconOnlyMode) {
            m_isIconOnlyMode = shouldBeIconOnly;
            updateModelButtonDisplay();
        }
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
} // namespace ui::widget::chat
