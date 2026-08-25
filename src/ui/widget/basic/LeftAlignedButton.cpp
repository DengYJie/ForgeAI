#include "LeftAlignedButton.h"

namespace ui::widget::basic {

QRectF LeftAlignedButton::contentPaintRect(const QRectF& surfaceRect) const {
    constexpr int kLeftMargin = 8;
    const auto& spacing = themeSpacing();
    const int iconW = iconGlyph().isEmpty() ? 0 : iconPixelSize();
    const int textW = text().isEmpty() ? 0 : fontMetrics().horizontalAdvance(text());
    const int gap = (!text().isEmpty() && !iconGlyph().isEmpty()) ? spacing.gap.tight : 0;
    const int totalW = textW + iconW + gap;

    return QRectF(surfaceRect.left() + kLeftMargin, surfaceRect.top(), totalW, surfaceRect.height());
}

} // namespace ui::widget::basic
