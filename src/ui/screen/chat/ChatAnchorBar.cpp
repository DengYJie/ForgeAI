#include "ChatAnchorBar.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QtMath>
#include <FluentQt/TextFields.h>
#include <FluentQt/Design.h>

namespace ui::screen::chat {
    namespace {
        constexpr int kBarWidth = 32;
        constexpr int kItemSpacing = 8;
        constexpr int kTopMargin = 8;
        constexpr qreal kNormalDashWidth = 6.0;
        constexpr qreal kNormalDashHeight = 2.0;
    } // namespace

    ChatAnchorPreviewCard::ChatAnchorPreviewCard(QWidget *parent)
        : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint),
          m_titleLabel(new fluent::textfields::Label(this)),
          m_bodyLabel(new fluent::textfields::Label(this)) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(4);

        m_titleLabel->setFluentTypography(Typography::FontRole::BodyStrong);
        m_titleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
        m_titleLabel->setWordWrap(true);
        layout->addWidget(m_titleLabel);

        m_bodyLabel->setFluentTypography(Typography::FontRole::Caption);
        m_bodyLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        m_bodyLabel->setWordWrap(true);
        m_bodyLabel->setMaximumWidth(260);
        layout->addWidget(m_bodyLabel);

        setFixedWidth(280);
    }

    void ChatAnchorPreviewCard::setContent(const QString &title, const QString &body) {
        m_titleLabel->setText(title);
        m_bodyLabel->setText(body);
        m_bodyLabel->setVisible(!body.isEmpty());
        adjustSize();
    }

    void ChatAnchorPreviewCard::onThemeUpdated() {
        if (m_titleLabel)
            m_titleLabel->onThemeUpdated();
        if (m_bodyLabel)
            m_bodyLabel->onThemeUpdated();
        update();
    }

    void ChatAnchorPreviewCard::paintEvent(QPaintEvent *event) {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const auto &colors = themeColorsRef();
        const QRectF cardRect = rect().adjusted(0.5, 0.5, -0.5, -0.5);

        painter.setPen(QPen(colors.strokeDefault, 1.0));
        painter.setBrush(colors.bgLayer);
        painter.drawRoundedRect(cardRect, 8, 8);
    }

    ChatAnchorBar::ChatAnchorBar(QWidget *parent)
        : QWidget(parent) {
        setFixedWidth(kBarWidth);
        setMouseTracking(true);

        m_hoverAnim = new QVariantAnimation(this);
        m_hoverAnim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_hoverAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
            m_mouseY = val.toReal();
            update();
        });

        m_previewCard = new ChatAnchorPreviewCard(this);
        m_previewCard->hide();
    }

    ChatAnchorBar::~ChatAnchorBar() = default;

    void ChatAnchorBar::setAnchors(const QList<ChatAnchorItem> &anchors) {
        m_items = anchors;
        if (m_activeIndex >= m_items.size()) {
            m_activeIndex = m_items.isEmpty() ? -1 : (m_items.size() - 1);
        }
        update();
    }

    void ChatAnchorBar::addAnchor(const QString &id, const QString &title, const QString &previewText) {
        m_items.append({id, title, previewText});
        update();
    }

    void ChatAnchorBar::removeAnchor(const QString &id) {
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items[i].id == id) {
                m_items.removeAt(i);
                break;
            }
        }
        if (m_activeIndex >= m_items.size()) {
            m_activeIndex = m_items.size() - 1;
        }
        update();
    }

    void ChatAnchorBar::clearAnchors() {
        m_items.clear();
        m_activeIndex = -1;
        m_hoveredIndex = -1;
        hidePreview();
        update();
    }

    void ChatAnchorBar::setActiveIndex(int index) {
        if (m_activeIndex == index)
            return;
        m_activeIndex = index;
        update();
    }

    void ChatAnchorBar::onThemeUpdated() {
        if (m_previewCard) {
            m_previewCard->onThemeUpdated();
        }
        update();
    }

    qreal ChatAnchorBar::contentTopOffset() const {
        if (m_items.isEmpty())
            return kTopMargin;

        const qreal totalSpan = (m_items.size() - 1) * kItemSpacing;
        if (height() > totalSpan + kTopMargin * 2) {
            return (height() - totalSpan) / 2.0;
        }
        return kTopMargin;
    }

    QRectF ChatAnchorBar::itemDashRect(int index) const {
        const qreal centerY = contentTopOffset() + index * kItemSpacing;
        qreal dashWidth = kNormalDashWidth;
        qreal dashHeight = kNormalDashHeight;

        if (m_hoverIntensity > 0.001) {
            const qreal dy = centerY - m_mouseY;
            const qreal sigma = kItemSpacing * 2.0;
            const qreal w = m_hoverIntensity * qExp(-(dy * dy) / (2.0 * sigma * sigma));
            dashWidth = kNormalDashWidth + w * (26.0 - kNormalDashWidth);
            dashHeight = kNormalDashHeight + w * (2.5 - kNormalDashHeight);
        }

        constexpr qreal startX = 4.0;
        const qreal startY = centerY - dashHeight / 2.0;
        return QRectF(startX, startY, dashWidth, dashHeight);
    }

    QRectF ChatAnchorBar::itemHitRect(int index) const {
        const qreal centerY = contentTopOffset() + index * kItemSpacing;
        return QRectF(0, centerY - (kItemSpacing / 2.0), width(), kItemSpacing);
    }

    int ChatAnchorBar::indexAtPosition(const QPoint &pos) const {
        for (int i = 0; i < m_items.size(); ++i) {
            if (itemHitRect(i).contains(pos)) {
                return i;
            }
        }
        return -1;
    }

    void ChatAnchorBar::paintEvent(QPaintEvent *event) {
        Q_UNUSED(event);
        if (m_items.isEmpty())
            return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const auto &colors = themeColorsRef();

        for (int i = 0; i < m_items.size(); ++i) {
            const QRectF r = itemDashRect(i);
            const qreal radius = r.height() / 2.0;

            const qreal centerY = contentTopOffset() + i * kItemSpacing;
            const qreal dy = centerY - m_mouseY;
            const qreal sigma = kItemSpacing * 2.0;
            const qreal w = (m_hoverIntensity > 0.001)
                                ? m_hoverIntensity * qExp(-(dy * dy) / (2.0 * sigma * sigma))
                                : 0.0;

            QColor base;
            if (i == m_activeIndex) {
                base = colors.textPrimary;
                base.setAlpha(220);
            } else {
                base = colors.textSecondary;
                base.setAlpha(60);
            }

            // 高斯波叠加：距鼠标越近越亮
            QColor bright = colors.textPrimary;
            const int blendedAlpha = qRound(base.alpha() + w * (255 - base.alpha()));
            QColor color;
            color.setRgbF(
                base.redF()   + w * (bright.redF()   - base.redF()),
                base.greenF() + w * (bright.greenF() - base.greenF()),
                base.blueF()  + w * (bright.blueF()  - base.blueF())
            );
            color.setAlpha(blendedAlpha);

            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawRoundedRect(r, radius, radius);
        }
    }

    void ChatAnchorBar::mouseMoveEvent(QMouseEvent *event) {
        const int idx = indexAtPosition(event->pos());
        if (idx >= 0) {
            setCursor(Qt::PointingHandCursor);
        } else {
            unsetCursor();
        }

        // 用 themeAnimation().fast 驱动高斯波中心平滑跟踪鼠标 Y 坐标
        const qreal targetY = static_cast<qreal>(event->pos().y());
        m_hoverAnim->stop();
        m_hoverAnim->setDuration(themeAnimation().fast);
        m_hoverAnim->setStartValue(m_mouseY < 0 ? targetY : m_mouseY);
        m_hoverAnim->setEndValue(targetY);
        m_hoverAnim->start();
        m_hoverIntensity = 1.0;

        if (idx != m_hoveredIndex) {
            m_hoveredIndex = idx;
            if (idx >= 0) {
                showPreviewAt(idx);
            } else {
                hidePreview();
            }
        }
    }

    void ChatAnchorBar::mousePressEvent(QMouseEvent *event) {
        if (event->button() == Qt::LeftButton) {
            const int idx = indexAtPosition(event->pos());
            if (idx >= 0) {
                setActiveIndex(idx);
                emit anchorClicked(idx, m_items[idx].id);
            }
        }
    }

    void ChatAnchorBar::leaveEvent(QEvent *event) {
        Q_UNUSED(event);
        m_hoveredIndex = -1;
        unsetCursor();
        hidePreview();

        // 用动画让高斯波强度平滑淡出，而不是瞬间消失
        m_hoverAnim->stop();
        m_hoverAnim->setDuration(themeAnimation().normal);
        m_hoverAnim->setStartValue(m_mouseY);
        m_hoverAnim->setEndValue(m_mouseY); // Y不变，仅通过 m_hoverIntensity 控制淡出
        // 单独用一个小定时器递减强度
        const auto startIntensity = m_hoverIntensity;
        auto *fadeAnim = new QVariantAnimation(this);
        fadeAnim->setDuration(themeAnimation().normal);
        fadeAnim->setEasingCurve(QEasingCurve::OutCubic);
        fadeAnim->setStartValue(startIntensity);
        fadeAnim->setEndValue(0.0);
        connect(fadeAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
            m_hoverIntensity = val.toReal();
            update();
        });
        connect(fadeAnim, &QVariantAnimation::finished, fadeAnim, &QObject::deleteLater);
        fadeAnim->start();
    }

    void ChatAnchorBar::showPreviewAt(int index) {
        if (index < 0 || index >= m_items.size() || !m_previewCard)
            return;

        const auto &item = m_items.at(index);
        m_previewCard->setContent(item.title, item.previewText);

        const qreal centerY = contentTopOffset() + index * kItemSpacing;
        const QPoint globalPos = mapToGlobal(QPoint(width() + 4, qRound(centerY) - m_previewCard->height() / 2));
        m_previewCard->move(globalPos);
        m_previewCard->show();
        m_previewCard->raise();
    }

    void ChatAnchorBar::hidePreview() {
        if (m_previewCard && m_previewCard->isVisible()) {
            m_previewCard->hide();
        }
    }
} // namespace ui::screen::chat
