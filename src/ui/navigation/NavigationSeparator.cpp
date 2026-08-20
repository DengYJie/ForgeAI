#include "ui/navigation/NavigationSeparator.h"
#include <QPainter>

#include "ui/navigation/NavigationMetrics.h"

namespace ui::navigation {
    NavigationSeparator::NavigationSeparator(QWidget *parent)
        : NavigationWidget(false, parent) {
        setCursor(Qt::ArrowCursor);
        setFocusPolicy(Qt::NoFocus);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        // 覆盖基类 setFixedHeight(kItemHeight)：Left 模式分隔线占位 = 上留白3 + 线1 + 下留白4
        setFixedHeight(kSeparatorLeadingMargin + kSeparatorLineThickness + kSeparatorTrailingMargin);
    }

    void NavigationSeparator::setOrientation(Qt::Orientation orientation) {
        NavigationWidget::setOrientation(orientation);
        if (orientation == Qt::Horizontal) {
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            // Top 模式：左留白3 + 线1 + 右留白4，撑满栏高
            setFixedSize(kSeparatorLeadingMargin + kSeparatorLineThickness + kSeparatorTrailingMargin,
                         kTopBarItemHeight);
        } else {
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            setFixedHeight(kSeparatorLeadingMargin + kSeparatorLineThickness + kSeparatorTrailingMargin);
        }
        updateGeometry();
    }

    QSize NavigationSeparator::sizeHint() const {
        if (m_orientation == Qt::Horizontal) {
            return QSize(kSeparatorLeadingMargin + kSeparatorLineThickness + kSeparatorTrailingMargin,
                         kTopBarItemHeight);
        }
        return QSize(0, kSeparatorLeadingMargin + kSeparatorLineThickness + kSeparatorTrailingMargin);
    }

    void NavigationSeparator::paintEvent(QPaintEvent * /*event*/) {
        QPainter painter(this);
        const auto &colors = colorsRef();

        // WinUI3 NavigationViewItemSeparatorForeground = DividerStrokeColorDefaultBrush
        QPen pen(colors.strokeDivider);
        pen.setWidth(kSeparatorLineThickness);
        pen.setCosmetic(true);
        painter.setPen(pen);

        // 线通栏绘制，无左右/上下缩进，仅按前侧留白定位
        if (m_orientation == Qt::Horizontal) {
            painter.drawLine(kSeparatorLeadingMargin, 0, kSeparatorLeadingMargin, height());
        } else {
            painter.drawLine(0, kSeparatorLeadingMargin, width(), kSeparatorLeadingMargin);
        }
    }
} // namespace ui::navigation
