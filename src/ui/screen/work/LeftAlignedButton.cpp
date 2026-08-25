#include "LeftAlignedButton.h"

namespace ui::screen::work {

QRectF LeftAlignedButton::contentPaintRect(const QRectF& surfaceRect) const {
    constexpr int kLeftInset = 8;
    const auto& spacing = themeSpacing();
    const int iconWidth = iconGlyph().isEmpty() ? 0 : iconPixelSize();
    const int textWidth = text().isEmpty() ? 0 : fontMetrics().horizontalAdvance(text());
    const int gap = iconWidth && textWidth ? spacing.gap.tight : 0;
    return QRectF(surfaceRect.left() + kLeftInset, surfaceRect.top(),
                  static_cast<qreal>(iconWidth + gap + textWidth), surfaceRect.height());
}

} // namespace ui::screen::work
