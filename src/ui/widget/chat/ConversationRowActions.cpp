#include "ConversationRowActions.h"
#include <FluentQt/Design.h>
#include <FluentQt/Foundation.h>

namespace ui::widget::chat {
using namespace fluent;

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
                                   const fluent::FluentElement* themeSource,
                                   const QPoint& hoveredPos, bool isHovered, bool isSelected,
                                   bool isPinned) {
    const auto& colors = themeSource ? themeSource->themeColorsRef() : ThemeRegistry::instance().colors(option.palette.color(QPalette::Window).lightness() < 128);
    const bool showButtons = isHovered || isSelected;

    if (!showButtons) {
        if (isPinned) {
            const QRect pinRect = pinButtonRect(option.rect);
            painter->setPen(colors.textAccentPrimary);
            Typography::Icons::paintGlyph(*painter, pinRect, Typography::Icons::PinFill, 12, Qt::AlignCenter);
        }
        return;
    }

    const QRect archiveRect = archiveButtonRect(option.rect);
    const QRect pinRect = pinButtonRect(option.rect);

    const HitTarget hit = isHovered ? hitTest(option.rect, hoveredPos) : HitTarget::None;

    // 1. Draw Archive Button
    if (hit == HitTarget::Archive) {
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(isSelected ? colors.subtleTertiary : colors.subtleSecondary);
        painter->drawRoundedRect(archiveRect, 4, 4);
        painter->setPen(colors.textPrimary);
    } else {
        painter->setPen(colors.textSecondary);
    }
    Typography::Icons::paintGlyph(*painter, archiveRect, QString(QChar(0xE7B8)), 11, Qt::AlignCenter);

    // 2. Draw Pin Button
    if (hit == HitTarget::Pin) {
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(isSelected ? colors.subtleTertiary : colors.subtleSecondary);
        painter->drawRoundedRect(pinRect, 4, 4);
        painter->setPen(isPinned ? colors.textAccentPrimary : colors.textPrimary);
    } else {
        painter->setPen(isPinned ? colors.textAccentPrimary : colors.textSecondary);
    }
    Typography::Icons::paintGlyph(*painter, pinRect, isPinned ? Typography::Icons::PinFill : Typography::Icons::Pin, 12, Qt::AlignCenter);
}

} // namespace ui::widget::chat
