#include "ProviderNavigationItem.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <FluentQt/Design.h>

namespace ui::screen::settings::model_manager {

    ProviderNavigationItem::ProviderNavigationItem(const domain::model::ModelProvider &provider, QWidget *parent)
        : QWidget(parent), m_provider(provider) {
        setFixedHeight(40);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void ProviderNavigationItem::setProvider(const domain::model::ModelProvider &provider) {
        m_provider = provider;
        update();
    }

    void ProviderNavigationItem::setSelected(bool selected) {
        if (m_isSelected != selected) {
            m_isSelected = selected;
            update();
        }
    }

    void ProviderNavigationItem::paintEvent(QPaintEvent *) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const bool isDark = (effectiveTheme() == fluent::FluentElement::Dark);
        const auto &colors = themeColorsRef();

        QRectF rect = this->rect();
        rect.adjust(4, 2, -4, -2);

        // 1. 背景色
        QColor bgColor = Qt::transparent;
        if (m_isSelected) {
            bgColor = isDark ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 15);
        } else if (m_isHovered) {
            bgColor = isDark ? QColor(255, 255, 255, 12) : QColor(0, 0, 0, 8);
        }

        if (bgColor != Qt::transparent) {
            QPainterPath path;
            path.addRoundedRect(rect, 4, 4);
            painter.fillPath(path, bgColor);
        }

        // 2. 选中时左侧指示条
        if (m_isSelected) {
            QRectF indicatorRect(rect.left() + 2, rect.top() + (rect.height() - 16) / 2.0, 3, 16);
            QPainterPath indPath;
            indPath.addRoundedRect(indicatorRect, 1.5, 1.5);
            painter.fillPath(indPath, colors.accentDefault);
        }

        // 3. 启停状态小圆点
        qreal dotX = rect.left() + 14;
        qreal dotY = rect.top() + rect.height() / 2.0;
        QColor dotColor = m_provider.isEnabled ? QColor(16, 137, 62) : (isDark ? QColor(140, 140, 140) : QColor(160, 160, 160));
        painter.setPen(Qt::NoPen);
        painter.setBrush(dotColor);
        painter.drawEllipse(QPointF(dotX, dotY), 3.5, 3.5);

        // 4. 模型数量 Badge
        QString countStr = QString::number(m_provider.models.size());
        QFont captionFont = Typography::fontStyle(Typography::FontRole::Caption).toQFont();
        QFontMetrics fmCaption(captionFont);
        int badgeTextWidth = fmCaption.horizontalAdvance(countStr);
        int badgeWidth = qMax(18, badgeTextWidth + 8);
        int badgeHeight = 18;
        QRectF badgeRect(rect.right() - badgeWidth - 8, rect.top() + (rect.height() - badgeHeight) / 2.0, badgeWidth, badgeHeight);

        QPainterPath badgePath;
        badgePath.addRoundedRect(badgeRect, 9, 9);
        painter.fillPath(badgePath, isDark ? QColor(255, 255, 255, 14) : QColor(0, 0, 0, 10));

        painter.setFont(captionFont);
        painter.setPen(colors.textSecondary);
        painter.drawText(badgeRect, Qt::AlignCenter, countStr);

        // 5. 服务商名称文本
        QFont titleFont = m_isSelected 
            ? Typography::fontStyle(Typography::FontRole::BodyStrong).toQFont()
            : Typography::fontStyle(Typography::FontRole::Body).toQFont();
        painter.setFont(titleFont);
        painter.setPen(m_isSelected ? colors.textPrimary : colors.textPrimary);

        qreal textLeft = dotX + 10;
        qreal textRight = badgeRect.left() - 6;
        QRectF textRect(textLeft, rect.top(), qMax(0.0, textRight - textLeft), rect.height());
        
        QFontMetrics fm(titleFont);
        QString elidedName = fm.elidedText(m_provider.name, Qt::ElideRight, static_cast<int>(textRect.width()));
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elidedName);
    }

    void ProviderNavigationItem::mousePressEvent(QMouseEvent *event) {
        if (event->button() == Qt::LeftButton) {
            Q_EMIT clicked(m_provider.id);
        }
        QWidget::mousePressEvent(event);
    }

    void ProviderNavigationItem::enterEvent(QEnterEvent *) {
        m_isHovered = true;
        update();
    }

    void ProviderNavigationItem::leaveEvent(QEvent *) {
        m_isHovered = false;
        update();
    }

    void ProviderNavigationItem::onThemeUpdated() {
        update();
    }

} // namespace ui::screen::settings::model_manager
