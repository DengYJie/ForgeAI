#include "ProjectHeader.h"

#include <QPainter>
#include <QMouseEvent>
#include <QHelpEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QScreen>
#include <FluentQt/StatusInfo.h>

namespace ui::screen::work {

ProjectHeader::ProjectHeader(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setFixedHeight(32);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ProjectHeader::setTitle(const QString& title) {
    if (m_title != title) {
        m_title = title;
        update();
    }
}

void ProjectHeader::setExpanded(bool expanded) {
    if (m_isExpanded != expanded) {
        m_isExpanded = expanded;
        update();
    }
}

QSize ProjectHeader::sizeHint() const {
    return QSize(200, 32);
}

QSize ProjectHeader::minimumSizeHint() const {
    return QSize(100, 32);
}

QRect ProjectHeader::chevronRect() const {
    QFont font = themeFont(Typography::FontRole::Body).toQFont();
    font.setPixelSize(Typography::FontSize::Caption);
    const QFontMetrics fm(font);
    const int textWidth = fm.horizontalAdvance(m_title);
    constexpr int size = 20;
    return QRect(8 + textWidth + 2, (height() - size) / 2, size, size);
}

QRect ProjectHeader::addRect() const {
    constexpr int size = 24;
    return QRect(width() - 8 - size, (height() - size) / 2, size, size);
}

QRect ProjectHeader::moreRect() const {
    constexpr int size = 24;
    return QRect(width() - 8 - size - 4 - size, (height() - size) / 2, size, size);
}

ProjectHeader::ButtonType ProjectHeader::hitTest(const QPoint& pos) const {
    if (chevronRect().contains(pos)) return Chevron;
    if (moreRect().contains(pos)) return More;
    if (addRect().contains(pos)) return Add;
    return None;
}

void ProjectHeader::enterEvent(FluentEnterEvent* event) {
    m_isHovered = true;
    update();
    QWidget::enterEvent(event);
}

void ProjectHeader::leaveEvent(QEvent* event) {
    m_isHovered = false;
    m_hoverButton = None;
    m_pressButton = None;
    setCursor(Qt::ArrowCursor);
    update();
    QWidget::leaveEvent(event);
}

void ProjectHeader::mouseMoveEvent(QMouseEvent* event) {
    ButtonType btn = hitTest(event->pos());
    if (m_hoverButton != btn) {
        m_hoverButton = btn;
        setCursor(btn != None ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void ProjectHeader::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_pressButton = hitTest(event->pos());
        update();
    }
    QWidget::mousePressEvent(event);
}

void ProjectHeader::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        ButtonType released = hitTest(event->pos());
        if (m_pressButton == released && released != None) {
            if (released == Chevron) {
                m_isExpanded = !m_isExpanded;
                Q_EMIT expandToggled(m_isExpanded);
            } else if (released == Add) {
                Q_EMIT addProjectClicked();
            } else if (released == More) {
                Q_EMIT moreProjectsClicked();
            }
        }
        m_pressButton = None;
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void ProjectHeader::showToolTip(const QString& text, const QRect& targetRect) {
    if (text.isEmpty()) {
        hideToolTip();
        return;
    }

    auto* tooltip = qobject_cast<fluent::status_info::ToolTip*>(m_tooltip.data());
    if (!tooltip) {
        tooltip = new fluent::status_info::ToolTip(nullptr);
        tooltip->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        tooltip->setAnimationEnabled(true);
        m_tooltip = tooltip;
    }

    tooltip->setText(text);
    tooltip->setThemeSource(this);
    tooltip->adjustSize();

    const int shadowMargin = tooltip->shadowMargin();
    const QSize outerSize = tooltip->sizeHint().expandedTo(tooltip->size());
    const QSize cardSize(qMax(0, outerSize.width() - 2 * shadowMargin),
                         qMax(0, outerSize.height() - 2 * shadowMargin));

    const QPoint globalTopLeft = mapToGlobal(targetRect.topLeft());
    const QRect globalTargetRect(globalTopLeft, targetRect.size());

    QPoint visibleTopLeft(globalTargetRect.center().x() - cardSize.width() / 2,
                          globalTargetRect.top() - 6 - cardSize.height());

    QScreen* screen = QGuiApplication::screenAt(globalTargetRect.center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect avail = screen->availableGeometry();
        if (visibleTopLeft.y() < avail.top()) {
            visibleTopLeft.setY(globalTargetRect.bottom() + 6);
        }
        visibleTopLeft.setX(qBound(avail.left() + 4, visibleTopLeft.x(), avail.right() - cardSize.width() - 4));
        visibleTopLeft.setY(qBound(avail.top() + 4, visibleTopLeft.y(), avail.bottom() - cardSize.height() - 4));
    }

    tooltip->move(visibleTopLeft - QPoint(shadowMargin, shadowMargin));
    if (!tooltip->isVisible()) {
        tooltip->setVisible(true);
    }
    tooltip->raise();
}

void ProjectHeader::hideToolTip() {
    if (m_tooltip && m_tooltip->isVisible()) {
        m_tooltip->setVisible(false);
    }
}

bool ProjectHeader::event(QEvent* event) {
    if (event->type() == QEvent::ToolTip) {
        auto* helpEvent = static_cast<QHelpEvent*>(event);
        ButtonType btn = hitTest(helpEvent->pos());
        if (btn == Chevron) {
            showToolTip(m_isExpanded ? tr("折叠项目") : tr("展开项目"), chevronRect());
            return true;
        } else if (btn == More) {
            showToolTip(tr("项目操作"), moreRect());
            return true;
        } else if (btn == Add) {
            showToolTip(tr("添加项目"), addRect());
            return true;
        } else {
            hideToolTip();
            event->ignore();
            return true;
        }
    }
    return QWidget::event(event);
}

void ProjectHeader::onThemeUpdated() {
    update();
}

void ProjectHeader::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const auto& colors = themeColorsRef();

    // 1. Draw title
    QFont font = themeFont(Typography::FontRole::Body).toQFont();
    font.setPixelSize(Typography::FontSize::Caption);
    painter.setFont(font);
    painter.setPen(colors.textSecondary);

    const QFontMetrics fm(font);
    const int textWidth = fm.horizontalAdvance(m_title);
    const QRect titleRect(8, 0, textWidth, height());
    painter.drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft, m_title);

    // 2. Draw actions on hover or pressed
    if (m_isHovered || m_pressButton != None) {
        auto drawButton = [&](const QRect& rect, const QString& glyph, int iconSize, ButtonType type) {
            if (m_pressButton == type && m_hoverButton == type) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(colors.subtleTertiary);
                painter.drawRoundedRect(rect, 4, 4);
            } else if (m_hoverButton == type) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(colors.subtleSecondary);
                painter.drawRoundedRect(rect, 4, 4);
            }
            painter.setPen(colors.textSecondary);
            Typography::Icons::paintGlyph(painter, rect, glyph, iconSize, Qt::AlignCenter);
        };

        const QString chevronGlyph = m_isExpanded ? Typography::Icons::ChevronDown : Typography::Icons::ChevronRight;
        drawButton(chevronRect(), chevronGlyph, 10, Chevron);
        drawButton(moreRect(), Typography::Icons::More, 13, More);
        drawButton(addRect(), Typography::Icons::Add, 13, Add);
    }
}

} // namespace ui::screen::work
