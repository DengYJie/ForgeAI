#include "ui/navigation/NavigationSectionHeader.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>

#include "ui/navigation/NavigationMetrics.h"

namespace ui::navigation {
    NavigationSectionHeader::NavigationSectionHeader(const QString &text, QWidget *parent)
        : NavigationWidget(/*isSelectable=*/false, parent)
          , m_text(text) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFocusPolicy(Qt::NoFocus);
        updateCompactHeight();
    }

    void NavigationSectionHeader::setCompacted(bool compacted) {
        NavigationWidget::setCompacted(compacted);
        updateCompactHeight();
    }

    void NavigationSectionHeader::setOrientation(Qt::Orientation orientation) {
        NavigationWidget::setOrientation(orientation);
        // 基类按通用行高设固定高度，header 需按自身规范（Vertical=40 折叠缩放 / Top=48）重算
        updateCompactHeight();
        updateGeometry();
    }

    QSize NavigationSectionHeader::sizeHint() const {
        if (m_orientation == Qt::Horizontal) {
            QFont f = themeFont(Typography::FontRole::Caption).toQFont();
            QFontMetrics fm(f);
            // 文字宽度 + 两侧内边距
            return QSize(fm.horizontalAdvance(m_text) + themeSpacing().medium * 2, kTopBarItemHeight);
        }
        return QSize(0, isCompacted() ? 0 : kSectionHeight);
    }

    void NavigationSectionHeader::updateCompactHeight() {
        // Top 模式（Horizontal）高度对齐栏高
        if (m_orientation == Qt::Horizontal) {
            setFixedHeight(kTopBarItemHeight);
            return;
        }
        setFixedHeight(isCompacted() ? 0 : kSectionHeight);
    }

    void NavigationSectionHeader::mousePressEvent(QMouseEvent *event) {
        // 分区标题不可点击：不进入 pressed 态，也不发 clicked
        event->ignore();
    }

    void NavigationSectionHeader::mouseReleaseEvent(QMouseEvent *event) {
        event->ignore();
    }

    void NavigationSectionHeader::paintEvent(QPaintEvent * /*event*/) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::TextAntialiasing);

        if (isCompacted())
            return;

        painter.setFont(themeFont(Typography::FontRole::Caption).toQFont());
        painter.setPen(colorsRef().textSecondary);

        // Horizontal 采用普通边距，Vertical 与图标左边缘对齐
        const int x = (m_orientation == Qt::Horizontal)
                          ? themeSpacing().medium
                          : (kRowLeftInset + kContentStart);
        const QRect textRect(x, 0, qMax(0, width() - x - themeSpacing().small), height());
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, m_text);
    }
} // namespace ui::navigation
