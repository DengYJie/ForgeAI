#pragma once

#include <QRect>
#include <QPainter>
#include <QStyleOptionViewItem>

namespace ui::widget::chat {

struct ConversationRowActions {
    // Layout configurations
    static constexpr int kButtonSize = 20;
    static constexpr int kButtonMargin = 4;

    static QRect archiveButtonRect(const QRect& itemRect);
    static QRect pinButtonRect(const QRect& itemRect);

    enum class HitTarget {
        None,
        Pin,
        Archive
    };

    static HitTarget hitTest(const QRect& itemRect, const QPoint& pos);

    static void paint(QPainter* painter, const QStyleOptionViewItem& option, 
                      const QPoint& hoveredPos, bool isHovered, bool isSelected,
                      bool isPinned);
};

} // namespace ui::widget::chat
