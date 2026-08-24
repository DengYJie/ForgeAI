#include "ModelTreeItemDelegate.h"

#include <QPainter>
#include <QPainterPath>
#include <QIcon>
#include <FluentQt/Collections.h>
#include <FluentQt/Design.h>

#include "domain/model/ModelCapabilities.h"
#include "ui/widget/badge/ModelCapabilityStyle.h"

namespace ui::screen::settings::model_manager {

    namespace {

        QPainterPath makeCardFillPath(const QRectF &rect, qreal tl, qreal tr, qreal br, qreal bl) {
            QPainterPath path;
            path.moveTo(rect.left() + tl, rect.top());
            path.lineTo(rect.right() - tr, rect.top());
            if (tr > 0) path.arcTo(QRectF(rect.right() - 2 * tr, rect.top(), 2 * tr, 2 * tr), 90, -90);
            path.lineTo(rect.right(), rect.bottom() - br);
            if (br > 0) path.arcTo(QRectF(rect.right() - 2 * br, rect.bottom() - 2 * br, 2 * br, 2 * br), 0, -90);
            path.lineTo(rect.left() + bl, rect.bottom());
            if (bl > 0) path.arcTo(QRectF(rect.left(), rect.bottom() - 2 * bl, 2 * bl, 2 * bl), 270, -90);
            path.lineTo(rect.left(), rect.top() + tl);
            if (tl > 0) path.arcTo(QRectF(rect.left(), rect.top(), 2 * tl, 2 * tl), 180, -90);
            path.closeSubpath();
            return path;
        }

        QPainterPath makeTopCardBorderPath(const QRectF &rect, qreal r) {
            QPainterPath path;
            path.moveTo(rect.left(), rect.bottom());
            path.lineTo(rect.left(), rect.top() + r);
            path.arcTo(QRectF(rect.left(), rect.top(), 2 * r, 2 * r), 180, -90);
            path.lineTo(rect.right() - r, rect.top());
            path.arcTo(QRectF(rect.right() - 2 * r, rect.top(), 2 * r, 2 * r), 90, -90);
            path.lineTo(rect.right(), rect.bottom());
            return path;
        }

        QPainterPath makeBottomCardBorderPath(const QRectF &rect, qreal r) {
            QPainterPath path;
            path.moveTo(rect.left(), rect.top());
            path.lineTo(rect.left(), rect.bottom() - r);
            path.arcTo(QRectF(rect.left(), rect.bottom() - 2 * r, 2 * r, 2 * r), 180, 90);
            path.lineTo(rect.right() - r, rect.bottom());
            path.arcTo(QRectF(rect.right() - 2 * r, rect.bottom() - 2 * r, 2 * r, 2 * r), 270, 90);
            path.lineTo(rect.right(), rect.top());
            return path;
        }

    } // namespace

    ModelTreeItemDelegate::ModelTreeItemDelegate(fluent::collections::TreeView *treeView, QObject *parent)
        : QStyledItemDelegate(parent), m_treeView(treeView) {}

    QSize ModelTreeItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
        const auto spacing = themeSpacing();
        const bool isGroup = !index.parent().isValid();
        if (isGroup) {
            return QSize(option.rect.width(), spacing.controlHeight.large + spacing.xSmall);
        }
        const int rowCount = index.model()->rowCount(index.parent());
        const bool isLastChild = (index.row() == rowCount - 1);
        return QSize(option.rect.width(), isLastChild ? spacing.controlHeight.large + 2 : spacing.controlHeight.large);
    }

    void ModelTreeItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
        if (!index.isValid()) return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const auto &colors = themeColorsRef();
        const auto radius = themeRadius();
        const auto spacing = themeSpacing();
        const bool isHovered = (option.state & QStyle::State_MouseOver);
        const bool isGroup = !index.parent().isValid();

        if (isGroup) {
            // 1. 父节点（分组卡片头部）
            const bool isExpanded = m_treeView && m_treeView->isExpanded(index);
            const int childCount = index.model()->rowCount(index);
            const bool hasChildrenAndExpanded = (isExpanded && childCount > 0);

            QRectF cardRect;

            if (hasChildrenAndExpanded) {
                // 展开状态：顶部圆角，底部直边无缝连接子项
                cardRect = QRectF(option.rect).adjusted(2, 2, -2, 0);

                QPainterPath fillPath = makeCardFillPath(cardRect, radius.overlay, radius.overlay,
                                                         radius.none, radius.none);
                painter->fillPath(fillPath, colors.bgLayerAlt);

                if (isHovered) {
                    painter->fillPath(fillPath, colors.subtleSecondary);
                }

                painter->setPen(QPen(colors.strokeDefault, ::Spacing::Border::Normal));
                painter->drawPath(makeTopCardBorderPath(cardRect, radius.overlay));
            } else {
                // 折叠状态：四周独立圆角卡片
                cardRect = QRectF(option.rect).adjusted(2, 2, -2, -2);
                QPainterPath cardPath = makeCardFillPath(cardRect, radius.overlay, radius.overlay,
                                                         radius.overlay, radius.overlay);
                painter->fillPath(cardPath, colors.bgLayerAlt);

                if (isHovered) {
                    painter->fillPath(cardPath, colors.subtleSecondary);
                }

                painter->setPen(QPen(colors.strokeDefault, ::Spacing::Border::Normal));
                painter->drawPath(cardPath);
            }

            // 动态旋转 Chevron 箭头 (0° ~ 90°)
            const qreal rotation = m_treeView ? m_treeView->chevronRotation(index) : 0.0;
            painter->save();
            painter->translate(cardRect.left() + spacing.padding.card + spacing.gap.tight, cardRect.center().y());
            painter->rotate(rotation * 90.0);

            QFont iconFont(Typography::FontFamily::FluentIcons);
            iconFont.setPixelSize(Typography::IconSize::Compact);
            painter->setFont(iconFont);
            painter->setPen(colors.textSecondary);
            painter->drawText(QRectF(-spacing.small, -spacing.small, spacing.standard, spacing.standard),
                              Qt::AlignCenter, Typography::Icons::ChevronRight);
            painter->restore();

            // 分组标题
            painter->setFont(themeFont(Typography::FontRole::BodyStrong).toQFont());
            painter->setPen(colors.textPrimary);
            painter->drawText(QRectF(cardRect.left() + spacing.xLarge + spacing.gap.tight,
                                     cardRect.top(),
                                     cardRect.width() - spacing.xxLarge - (isHovered ? spacing.xLarge : 0),
                                     cardRect.height()),
                              Qt::AlignLeft | Qt::AlignVCenter, index.data(Qt::DisplayRole).toString());

            // 头部悬停时显示的减号操作按钮
            if (isHovered) {
                QFont actionFont(Typography::FontFamily::FluentIcons);
                actionFont.setPixelSize(Typography::IconSize::Standard);
                painter->setFont(actionFont);
                painter->setPen(colors.textSecondary);
                const qreal btnWidth = spacing.standard + spacing.gap.tight;
                const QRectF minusRect(cardRect.right() - spacing.standard - btnWidth,
                                       cardRect.top(),
                                       btnWidth,
                                       cardRect.height());
                painter->drawText(minusRect, Qt::AlignCenter, Typography::Icons::ChromeMinimize);
            }

        } else {
            // 2. 子节点（展开区域内的模型行卡片）
            const int rowCount = index.model()->rowCount(index.parent());
            const bool isLastChild = (index.row() == rowCount - 1);

            QRectF rowRect;

            if (isLastChild) {
                // 最后一行：底部圆角收尾
                rowRect = QRectF(option.rect).adjusted(2, 0, -2, -2);
                QPainterPath fillPath = makeCardFillPath(rowRect, radius.none, radius.none,
                                                         radius.overlay, radius.overlay);
                painter->fillPath(fillPath, colors.bgLayer);

                painter->setPen(QPen(colors.strokeDefault, ::Spacing::Border::Normal));
                painter->drawPath(makeBottomCardBorderPath(rowRect, radius.overlay));
            } else {
                // 中间行：矩形连续卡片体
                rowRect = QRectF(option.rect).adjusted(2, 0, -2, 0);
                painter->fillRect(rowRect, colors.bgLayer);

                painter->setPen(QPen(colors.strokeDefault, ::Spacing::Border::Normal));
                painter->drawLine(QPointF(rowRect.left(), rowRect.top()), QPointF(rowRect.left(), rowRect.bottom()));
                painter->drawLine(QPointF(rowRect.right(), rowRect.top()), QPointF(rowRect.right(), rowRect.bottom()));
            }

            // 左侧品牌圆形 Logo
            const QRectF iconRect(rowRect.left() + spacing.large, rowRect.center().y() - 11, 22, 22);
            const QVariant iconData = index.data(BrandIconRole);
            if (iconData.isValid() && iconData.canConvert<QIcon>()) {
                iconData.value<QIcon>().paint(painter, iconRect.toRect());
            }

            // 模型名称
            const qreal textLeft = iconRect.right() + spacing.gap.normal + 2;
            const qreal textRight = rowRect.right() - 180;
            const QString name = index.data(Qt::DisplayRole).toString();

            QFont titleFont = themeFont(Typography::FontRole::Body).toQFont();
            QFontMetrics fm(titleFont);
            QString elided = fm.elidedText(name, Qt::ElideRight, qMax(10, static_cast<int>(textRight - textLeft)));

            painter->setFont(titleFont);
            painter->setPen(colors.textPrimary);
            painter->drawText(QRectF(textLeft, rowRect.top(), textRight - textLeft, rowRect.height()),
                              Qt::AlignLeft | Qt::AlignVCenter, elided);

            // 右侧区域 (从右向左排布)
            qreal rightCursor = rowRect.right() - spacing.standard;

            // 操作按钮 (··· 与 ⚙)
            QFont actionFont(Typography::FontFamily::FluentIcons);
            actionFont.setPixelSize(Typography::IconSize::Standard);
            painter->setFont(actionFont);
            painter->setPen(colors.textSecondary);

            // 减号操作按钮 (-)
            painter->drawText(QRectF(rightCursor - spacing.standard - 2, rowRect.top(), spacing.standard + 2, rowRect.height()),
                              Qt::AlignCenter, Typography::Icons::ChromeMinimize);
            rightCursor -= spacing.large;

            // 齿轮设置按钮 (⚙)
            painter->drawText(QRectF(rightCursor - spacing.standard - 2, rowRect.top(), spacing.standard + 2, rowRect.height()),
                              Qt::AlignCenter, Typography::Icons::Settings);
            rightCursor -= (spacing.large + spacing.gap.tight);

            // 渲染能力徽标
            const int caps = index.data(ModelCapabilitiesRole).toInt();
            const auto &featuredCaps = ui::widget::badge::ModelCapabilityStyle::featuredBadgeCapabilities();

            for (const auto cap : featuredCaps) {
                if (caps & static_cast<int>(cap)) {
                    const QRectF badgeRect(rightCursor - ui::widget::badge::ModelCapabilityStyle::kDefaultBadgeSize.width(),
                                           rowRect.center().y() - (ui::widget::badge::ModelCapabilityStyle::kDefaultBadgeSize.height() / 2.0),
                                           ui::widget::badge::ModelCapabilityStyle::kDefaultBadgeSize.width(),
                                           ui::widget::badge::ModelCapabilityStyle::kDefaultBadgeSize.height());
                    ui::widget::badge::ModelCapabilityStyle::paintBadge(painter, badgeRect, cap, this);
                    rightCursor -= (ui::widget::badge::ModelCapabilityStyle::kDefaultBadgeSize.width() + spacing.gap.normal);
                }
            }
        }

        painter->restore();
    }

} // namespace ui::screen::settings::model_manager
