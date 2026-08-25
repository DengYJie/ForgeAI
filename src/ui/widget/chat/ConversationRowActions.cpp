#include "ConversationRowActions.h"
#include <FluentQt/Design.h>

namespace ui::widget::chat {

QRect ConversationRowActions::archiveButtonRect(const QRect& itemRect) {
    const int right = itemRect.right() - kButtonMargin;
    return QRect(right - kButtonSize,
                 itemRect.top() + (itemRect.height() - kButtonSize) / 2,
                 kButtonSize, kButtonSize);
}

QRect ConversationRowActions::pinButtonRect(const QRect& itemRect) {
    const QRect arcRect = archiveButtonRect(itemRect);
    return QRect(arcRect.left() - kButtonSize - 2,
                 arcRect.top(),
                 kButtonSize, kButtonSize);
}

ConversationRowActions::HitTarget ConversationRowActions::hitTest(const QRect& itemRect, const QPoint& pos) {
    if (archiveButtonRect(itemRect).contains(pos)) {
        return HitTarget::Archive;
    }
    if (pinButtonRect(itemRect).contains(pos)) {
        return HitTarget::Pin;
    }
    return HitTarget::None;
}

void ConversationRowActions::paint(QPainter* painter, const QStyleOptionViewItem& option, 
                                   const QPoint& hoveredPos, bool isHovered, bool isSelected,
                                   bool isPinned) {
    // Both Chat and Work use the rule: Actions are visible if the item is selected OR hovered.
    // Wait, the user mentioned: "Chat 与 Work 的按钮可用性不一致：Chat 已选中行也显示置顶/归档按钮；Work 仅悬浮显示。"
    // The user suggests extracting to unify this. Let's make it visible if isHovered || isSelected.
    if (!isHovered && !isSelected) {
        // If not hovered and not selected, only show Pin if it is actually pinned, and don't show archive.
        if (isPinned) {
            const QRect pinRect = pinButtonRect(option.rect);
            painter->setFont(QFont(Typography::FontFamily::FluentIcons, 10));
            painter->setPen(option.palette.color(QPalette::Highlight));
            painter->drawText(pinRect, Qt::AlignCenter, Typography::Icons::PinFill);
        }
        return;
    }

    const QRect archiveRect = archiveButtonRect(option.rect);
    const QRect pinRect = pinButtonRect(option.rect);
    const QColor secondaryText = option.palette.color(QPalette::Text);
    QColor hoverFill = option.palette.color(QPalette::AlternateBase);
    hoverFill.setAlpha(96);

    painter->setFont(QFont(Typography::FontFamily::FluentIcons, 10));

    // Hover background for buttons
    const HitTarget hit = hitTest(option.rect, hoveredPos);
    if (hit != HitTarget::None) {
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(hoverFill);
        const QRect hitRect = (hit == HitTarget::Archive) ? archiveRect : pinRect;
        painter->drawRoundedRect(hitRect, 3, 3);
    }

    // Draw Archive
    painter->setPen(secondaryText);
    painter->drawText(archiveRect, Qt::AlignCenter, QString(QChar(0xE7B8)));

    // Draw Pin
    painter->setPen(isPinned ? option.palette.color(QPalette::Highlight) : secondaryText);
    painter->drawText(pinRect, Qt::AlignCenter, isPinned ? Typography::Icons::PinFill : Typography::Icons::Pin);
}

} // namespace ui::widget::chat
