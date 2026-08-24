#include "ProviderItemDelegate.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <FluentQt/Design.h>

#include "ProviderListModel.h"

namespace ui::screen::settings::model_manager {

    ProviderItemDelegate::ProviderItemDelegate(QObject *parent)
        : QStyledItemDelegate(parent) {}

    QSize ProviderItemDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const {
        return QSize(240, 40);
    }

    void ProviderItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
        if (!index.isValid() || !painter) return;

        // 过滤 FluentQt ListView 触发的重复叠加 Pressed 反馈 pass，避免产生错位重绘与双按钮
        if (option.state & QStyle::State_Sunken) {
            return;
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const auto &colors = themeColorsRef();
        const auto radius = themeRadius();
        const bool isDark = (effectiveTheme() == fluent::FluentElement::Dark);

        const QRectF rect = QRectF(option.rect).adjusted(2.0, 1.0, -2.0, -1.0);

        const bool isSelected = (option.state & QStyle::State_Selected);
        const bool isHovered = (option.state & QStyle::State_MouseOver);

        // 1. 条目背景 (正常态透明，悬浮与选中时展示 Fluent Subtle 高亮)
        QColor bgColor = Qt::transparent;
        if (isHovered || isSelected) {
            bgColor = colors.subtleSecondary;
        }

        if (bgColor != Qt::transparent && bgColor.alpha() > 0) {
            QPainterPath path;
            path.addRoundedRect(rect, radius.control, radius.control);
            painter->setPen(Qt::NoPen);
            painter->setBrush(bgColor);
            painter->drawPath(path);
        }

        // 2. 图标与服务商名称
        const QString name = index.data(ProviderNameRole).toString();
        const bool isEnabled = index.data(ProviderEnabledRole).toBool();
        const QVariant iconData = index.data(Qt::DecorationRole);

        qreal cursorX = rect.left() + 16.0;

        // 若存在有效图标，绘制 20x20 图标并将文本光标后移 8px
        if (iconData.isValid()) {
            QIcon icon;
            if (iconData.canConvert<QIcon>()) {
                icon = iconData.value<QIcon>();
            } else if (iconData.canConvert<QPixmap>()) {
                icon = QIcon(iconData.value<QPixmap>());
            }
            if (!icon.isNull()) {
                const QRectF iconRect(cursorX, rect.top() + (rect.height() - 20.0) / 2.0, 20.0, 20.0);
                icon.paint(painter, iconRect.toRect());
                cursorX = iconRect.right() + 8.0;
            }
        }

        QFont titleFont = isSelected
                              ? Typography::fontStyle(Typography::FontRole::BodyStrong).toQFont()
                              : Typography::fontStyle(Typography::FontRole::Body).toQFont();
        painter->setFont(titleFont);
        painter->setPen(colors.textPrimary);

        const qreal textRight = rect.right() - 32.0;
        const QRectF textRect(cursorX, rect.top(), qMax(0.0, textRight - cursorX), rect.height());

        QFontMetrics fm(titleFont);
        const QString elidedName = fm.elidedText(name, Qt::ElideRight, static_cast<int>(textRect.width()));
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elidedName);

        // 4. 右侧状态指示：严格依据 m_hoveredIndex 悬停态显示竖向三点菜单按钮，默认态显示启停小圆点
        const int rightEdge = qRound(rect.right());
        const int centerY = qRound(rect.top() + rect.height() / 2.0);

        if (isHovered) {
            const QRect btnRect(rightEdge - 28, centerY - 12, 24, 24);
            QPainterPath btnPath;
            btnPath.addRoundedRect(QRectF(btnRect), radius.control, radius.control);
            painter->setPen(Qt::NoPen);
            painter->setBrush(colors.subtleTertiary);
            painter->drawPath(btnPath);

            // 绘制竖向三点 (⋮)
            const qreal cx = btnRect.center().x();
            const qreal cy = btnRect.center().y();
            painter->setPen(Qt::NoPen);
            painter->setBrush(colors.textPrimary);
            painter->drawEllipse(QPointF(cx, cy - 4.5), 1.4, 1.4);
            painter->drawEllipse(QPointF(cx, cy), 1.4, 1.4);
            painter->drawEllipse(QPointF(cx, cy + 4.5), 1.4, 1.4);
        } else {
            const qreal dotX = rightEdge - 16;
            const qreal dotY = centerY;
            const QColor dotColor = isEnabled ? QColor(16, 137, 62) : (isDark ? QColor(140, 140, 140) : QColor(160, 160, 160));
            painter->setPen(Qt::NoPen);
            painter->setBrush(dotColor);
            painter->drawEllipse(QPointF(dotX, dotY), 3.5, 3.5);
        }

        painter->restore();
    }

} // namespace ui::screen::settings::model_manager
