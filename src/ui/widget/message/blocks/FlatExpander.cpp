#include "FlatExpander.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include <FluentQt/Design.h>
#include <FluentQt/Foundation.h>

namespace ui::widget::message::blocks {

/**
 * @brief 自绘 Fluent 风格头部栏
 */
class FlatExpanderHeader : public QWidget, public fluent::FluentElement {
public:
    explicit FlatExpanderHeader(FlatExpander *owner)
        : QWidget(owner)
        , m_owner(owner)
    {
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
        setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }

    QSize sizeHint() const override {
        return calculateSize();
    }

    QSize minimumSizeHint() const override {
        return calculateSize();
    }

    QSize calculateSize() const {
        if (!m_owner) return QSize(0, 26);
        QFont textFont = themeFont(Typography::FontRole::Caption).toQFont();
        QFontMetrics fm(textFont);
        QFont subFont(QStringLiteral("Consolas"), 9);
        QFontMetrics subFm(subFont);

        int w = 8 + 8; // left & right margins
        if (m_owner->chevronPosition() == FlatExpander::ChevronPosition::Left) {
            w += 14 + 6;
        }
        if (!m_owner->m_leadingGlyph.isEmpty()) {
            w += 16 + 6;
        }
        if (!m_owner->m_title.isEmpty()) {
            w += fm.horizontalAdvance(m_owner->m_title) + 6;
        }
        if (!m_owner->m_subtitle.isEmpty()) {
            w += subFm.horizontalAdvance(m_owner->m_subtitle) + 6;
        }
        if (m_owner->chevronPosition() == FlatExpander::ChevronPosition::InlineRight) {
            w += 14;
        }
        return QSize(w, m_owner->headerHeight());
    }

    void onThemeUpdated() override {
        update();
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
    }

    void enterEvent(QEnterEvent *) override {
        m_hovered = true;
        update();
    }

    void leaveEvent(QEvent *) override {
        m_hovered = false;
        m_pressed = false;
        update();
    }

    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton) {
            m_pressed = true;
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton && m_pressed) {
            m_pressed = false;
            update();
            if (rect().contains(e->pos()) && m_owner) {
                m_owner->toggleExpanded();
            }
        }
    }

    void paintEvent(QPaintEvent *) override {
        if (!m_owner) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const auto &colors = themeColorsRef();

        const bool isActive = m_hovered || m_pressed;
        const QColor titleColor = isActive ? colors.textPrimary : colors.textSecondary;
        const QColor subColor = isActive ? colors.textSecondary : colors.textTertiary;
        const QColor iconColor = isActive ? colors.textPrimary : colors.textSecondary;
        const QColor chevronColor = isActive ? colors.textPrimary : colors.textSecondary;

        // 1. Subtle 悬浮与按下背景
        if (m_pressed) {
            p.setBrush(colors.subtleTertiary);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);
        } else if (m_hovered) {
            p.setBrush(colors.subtleSecondary);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);
        }

        const int h = height();
        const int cy = h / 2;
        int x = 8;

        // 2. 左侧 Chevron 箭头
        if (m_owner->chevronPosition() == FlatExpander::ChevronPosition::Left) {
            drawChevron(p, QRectF(x, cy - 7, 14, 14), chevronColor);
            x += 14 + 6;
        }

        // 3. 前置 Fluent 图标 (FontIcon Glyph)
        if (!m_owner->m_leadingGlyph.isEmpty()) {
            QRectF iconRect(x, cy - 8, 16, 16);
            p.setPen(iconColor);
            Typography::Icons::paintGlyph(p, iconRect, m_owner->m_leadingGlyph, m_owner->m_leadingIconSize, Qt::AlignCenter);
            x += 16 + 6;
        }

        // 4. 标题文字
        if (!m_owner->m_title.isEmpty()) {
            QFont textFont = themeFont(Typography::FontRole::Caption).toQFont();
            QFontMetrics fm(textFont);
            p.setFont(textFont);
            p.setPen(titleColor);
            int textW = fm.horizontalAdvance(m_owner->m_title);
            QRect textRect(x, 0, textW, h);
            p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, m_owner->m_title);
            x += textW + 6;
        }

        // 5. 次要说明文字 (如状态已完成)
        if (!m_owner->m_subtitle.isEmpty()) {
            QFont subFont(QStringLiteral("Consolas"), 9);
            QFontMetrics subFm(subFont);
            p.setFont(subFont);
            p.setPen(subColor);
            int subW = subFm.horizontalAdvance(m_owner->m_subtitle);
            QRect subRect(x, 0, subW, h);
            p.drawText(subRect, Qt::AlignLeft | Qt::AlignVCenter, m_owner->m_subtitle);
            x += subW + 6;
        }

        // 6. 右侧 Chevron 箭头 (InlineRight 或 FarRight)
        if (m_owner->chevronPosition() == FlatExpander::ChevronPosition::InlineRight) {
            drawChevron(p, QRectF(x, cy - 7, 14, 14), chevronColor);
        } else if (m_owner->chevronPosition() == FlatExpander::ChevronPosition::FarRight) {
            drawChevron(p, QRectF(width() - 14 - 8, cy - 7, 14, 14), chevronColor);
        }
    }

private:
    void drawChevron(QPainter &p, const QRectF &rect, const QColor &color) {
        p.save();
        p.setPen(color);
        qreal angle = m_owner->m_fraction * 180.0;
        if (!qFuzzyIsNull(angle)) {
            p.translate(rect.center());
            p.rotate(angle);
            p.translate(-rect.center());
        }
        Typography::Icons::paintGlyph(p, rect, Typography::Icons::ChevronDown, Typography::IconSize::Compact, Qt::AlignCenter);
        p.restore();
    }

    FlatExpander *m_owner = nullptr;
    bool m_hovered = false;
    bool m_pressed = false;
};

FlatExpander::FlatExpander(const QString &title, int headerHeight, QWidget *parent)
    : QWidget(parent)
    , m_headerHeight(headerHeight)
    , m_title(title)
{
    setupUi();
}

FlatExpander::~FlatExpander() = default;

void FlatExpander::setupUi()
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 纯粹自绘头部
    m_headerBar = new FlatExpanderHeader(this);
    m_headerBar->setFixedHeight(m_headerHeight);
    mainLayout->addWidget(m_headerBar, 0, m_headerCompact ? Qt::AlignLeft : Qt::Alignment());

    // 折叠裁切区域
    m_clip = new QWidget(this);
    m_clip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_clip->setFixedHeight(0);
    m_clip->hide();

    m_clipLayout = new QVBoxLayout(m_clip);
    m_clipLayout->setContentsMargins(0, 0, 0, 0);
    m_clipLayout->setSpacing(0);

    mainLayout->addWidget(m_clip);

    // 动画引擎
    m_animation = new QVariantAnimation(this);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
        applyFraction(val.toReal());
    });
    connect(m_animation, &QVariantAnimation::finished, this, [this]() {
        applyFraction(m_expanded ? 1.0 : 0.0);
        if (!m_expanded) {
            m_clip->hide();
            setFixedHeight(m_headerHeight);
        } else {
            m_clip->show();
            m_contentTargetHeight = naturalContentHeight();
            m_clip->setFixedHeight(m_contentTargetHeight);
            setFixedHeight(m_headerHeight + m_contentTargetHeight);
        }
        updateGeometry();
        emit expansionFinished(m_expanded);
        emit contentHeightChanged();
    });

    setFixedHeight(m_headerHeight);
}

void FlatExpander::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void FlatExpander::setTitle(const QString &title)
{
    if (m_title == title) return;
    m_title = title;
    if (m_headerBar) {
        m_headerBar->updateGeometry();
        m_headerBar->update();
    }
    updateGeometry();
}

void FlatExpander::setSubtitle(const QString &subtitle)
{
    if (m_subtitle == subtitle) return;
    m_subtitle = subtitle;
    if (m_headerBar) {
        m_headerBar->updateGeometry();
        m_headerBar->update();
    }
    updateGeometry();
}

void FlatExpander::setLeadingIcon(const QString &fluentGlyph, int iconSize)
{
    m_leadingGlyph = fluentGlyph;
    m_leadingIconSize = iconSize;
    if (m_headerBar) {
        m_headerBar->updateGeometry();
        m_headerBar->update();
    }
    updateGeometry();
}

void FlatExpander::setChevronPosition(ChevronPosition pos)
{
    m_chevronPos = pos;
    if (m_headerBar) {
        m_headerBar->updateGeometry();
        m_headerBar->update();
    }
}

void FlatExpander::setHeaderCompact(bool compact)
{
    m_headerCompact = compact;
    if (auto *mainLay = qobject_cast<QVBoxLayout*>(layout())) {
        mainLay->setAlignment(m_headerBar, compact ? Qt::AlignLeft : Qt::Alignment());
    }
    if (m_headerBar) {
        m_headerBar->setSizePolicy(compact ? QSizePolicy::Minimum : QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_headerBar->updateGeometry();
        m_headerBar->update();
    }
}

void FlatExpander::setHeaderHeight(int h)
{
    m_headerHeight = h;
    if (m_headerBar) {
        m_headerBar->setFixedHeight(h);
    }
    updateLayout();
}

void FlatExpander::setContentWidget(QWidget *widget)
{
    if (m_contentWidget) {
        m_clipLayout->removeWidget(m_contentWidget);
        m_contentWidget->setParent(nullptr);
    }
    m_contentWidget = widget;
    if (m_contentWidget) {
        m_clipLayout->addWidget(m_contentWidget);
        if (m_expanded) {
            m_clip->show();
            m_contentTargetHeight = naturalContentHeight();
            m_clip->setFixedHeight(m_contentTargetHeight);
            setFixedHeight(m_headerHeight + m_contentTargetHeight);
        }
    }
    updateLayout();
}

void FlatExpander::setExpanded(bool expanded, bool animated)
{
    if (m_expanded == expanded && m_animation->state() != QAbstractAnimation::Running) return;

    m_animation->stop();

    if (expanded) {
        m_clip->show();
        if (width() > 0 && m_contentWidget) {
            m_contentWidget->resize(width(), m_contentWidget->height());
        }
        m_contentTargetHeight = naturalContentHeight();
    } else {
        if (m_contentTargetHeight == 0) {
            m_contentTargetHeight = naturalContentHeight();
        }
    }

    m_expanded = expanded;
    emit expandedChanged(m_expanded);

    const qreal targetFraction = m_expanded ? 1.0 : 0.0;
    if (!animated) {
        applyFraction(targetFraction);
        if (!m_expanded) {
            m_clip->hide();
            setFixedHeight(m_headerHeight);
        } else {
            m_clip->setFixedHeight(m_contentTargetHeight);
            setFixedHeight(m_headerHeight + m_contentTargetHeight);
        }
        updateGeometry();
        emit expansionFinished(m_expanded);
        emit contentHeightChanged();
        return;
    }

    const auto motion = themeAnimation();
    m_animation->setDuration(180);
    m_animation->setStartValue(m_fraction);
    m_animation->setEndValue(targetFraction);
    m_animation->setEasingCurve(motion.standard);
    m_animation->start();
}

void FlatExpander::toggleExpanded()
{
    setExpanded(!m_expanded);
}

void FlatExpander::forceUpdateContentHeight()
{
    if (m_expanded && m_animation->state() != QAbstractAnimation::Running) {
        m_contentTargetHeight = naturalContentHeight();
        m_clip->setFixedHeight(m_contentTargetHeight);
        setFixedHeight(m_headerHeight + m_contentTargetHeight);
        updateGeometry();
        emit contentHeightChanged();
    }
}

QSize FlatExpander::sizeHint() const
{
    int contentH = 0;
    if (m_contentWidget) {
        if (m_animation && m_animation->state() == QAbstractAnimation::Running) {
            contentH = qRound(m_contentTargetHeight * m_fraction);
        } else if (m_expanded) {
            contentH = m_contentTargetHeight > 0 ? m_contentTargetHeight : m_contentWidget->sizeHint().height();
        }
    }
    return QSize(200, m_headerHeight + contentH);
}

QSize FlatExpander::minimumSizeHint() const
{
    return QSize(0, m_headerHeight);
}

void FlatExpander::onThemeUpdated()
{
    if (m_headerBar) m_headerBar->onThemeUpdated();
}

int FlatExpander::naturalContentHeight() const
{
    if (!m_contentWidget) return 0;
    int h = m_contentWidget->sizeHint().height();
    if (m_contentWidget->hasHeightForWidth() && width() > 0) {
        h = m_contentWidget->heightForWidth(width());
    }
    return qMax(0, h);
}

void FlatExpander::applyFraction(qreal fraction)
{
    m_fraction = qBound(0.0, fraction, 1.0);
    int curContentH = qRound(m_contentTargetHeight * m_fraction);
    m_clip->setFixedHeight(curContentH);
    setFixedHeight(m_headerHeight + curContentH);
    if (m_headerBar) {
        m_headerBar->update();
    }
    updateGeometry();
    emit contentHeightChanged();
}

void FlatExpander::updateLayout()
{
    if (m_expanded) {
        forceUpdateContentHeight();
    } else {
        setFixedHeight(m_headerHeight);
        if (m_headerBar) m_headerBar->update();
        updateGeometry();
        emit contentHeightChanged();
    }
}

} // namespace ui::widget::message::blocks
