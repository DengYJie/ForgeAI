#include "ModelActionsSplitButton.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFontMetrics>

namespace ui::screen::settings::model_manager {

    ModelActionsSplitButton::ModelActionsSplitButton(QWidget *parent)
        : QWidget(parent) {
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        setFixedHeight(32);
        setCursor(Qt::PointingHandCursor);
    }

    void ModelActionsSplitButton::setRefreshing(bool refreshing) {
        if (m_isRefreshing != refreshing) {
            m_isRefreshing = refreshing;
            update();
        }
    }

    QSize ModelActionsSplitButton::sizeHint() const {
        const QString text = m_isRefreshing ? tr("正在获取...") : tr("获取模型列表");
        QFont font = Typography::fontStyle(Typography::FontRole::Body).toQFont();
        const QFontMetrics fm(font);
        const int textWidth = fm.horizontalAdvance(text);
        const int iconWidth = 14;
        const int gap = 8;
        const int padding = 24; // 12 on each side of left section
        const int leftWidth = iconWidth + gap + textWidth + padding;

        return QSize(leftWidth + m_rightWidth, 32);
    }

    QSize ModelActionsSplitButton::minimumSizeHint() const {
        return sizeHint();
    }

    ModelActionsSplitButton::Part ModelActionsSplitButton::hitTest(const QPoint &pos) const {
        if (!rect().contains(pos)) return None;
        if (pos.x() >= width() - m_rightWidth) return Right;
        return Left;
    }

    void ModelActionsSplitButton::mouseMoveEvent(QMouseEvent *event) {
        Part part = hitTest(event->pos());
        if (m_hoverPart != part) {
            m_hoverPart = part;
            update();
        }
        QWidget::mouseMoveEvent(event);
    }

    void ModelActionsSplitButton::mousePressEvent(QMouseEvent *event) {
        if (event->button() == Qt::LeftButton) {
            m_pressPart = hitTest(event->pos());
            update();
        }
        QWidget::mousePressEvent(event);
    }

    void ModelActionsSplitButton::mouseReleaseEvent(QMouseEvent *event) {
        if (event->button() == Qt::LeftButton) {
            Part releasePart = hitTest(event->pos());
            if (m_pressPart == Left && releasePart == Left && !m_isRefreshing) {
                Q_EMIT refreshRequested();
            } else if (m_pressPart == Right && releasePart == Right) {
                Q_EMIT addRequested();
            }
            m_pressPart = None;
            update();
        }
        QWidget::mouseReleaseEvent(event);
    }

    void ModelActionsSplitButton::leaveEvent(QEvent *event) {
        m_hoverPart = None;
        m_pressPart = None;
        update();
        QWidget::leaveEvent(event);
    }

    void ModelActionsSplitButton::onThemeUpdated() {
        update();
    }

    void ModelActionsSplitButton::paintEvent(QPaintEvent *) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const auto &colors = themeColorsRef();
        const auto &radius = themeRadius();

        const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
        const qreal cr = radius.control;

        QPainterPath basePath;
        basePath.addRoundedRect(r, cr, cr);

        // 1. 基础背景
        painter.fillPath(basePath, colors.controlDefault);

        // 2. 局部 Hover / Press 高亮
        const qreal divX = width() - m_rightWidth;
        const QRectF leftRect(r.left(), r.top(), divX - r.left(), r.height());
        const QRectF rightRect(divX, r.top(), r.right() - divX, r.height());

        painter.save();
        painter.setClipPath(basePath);

        if (m_pressPart == Left) {
            painter.fillRect(leftRect, colors.controlTertiary);
        } else if (m_hoverPart == Left && !m_isRefreshing) {
            painter.fillRect(leftRect, colors.controlSecondary);
        }

        if (m_pressPart == Right) {
            painter.fillRect(rightRect, colors.controlTertiary);
        } else if (m_hoverPart == Right) {
            painter.fillRect(rightRect, colors.controlSecondary);
        }
        painter.restore();

        // 3. 边框
        painter.setPen(colors.strokeDefault);
        painter.drawPath(basePath);

        // 4. 中间分割线 (1px)
        painter.setPen(colors.strokeDivider);
        painter.drawLine(QPointF(divX, r.top() + 4), QPointF(divX, r.bottom() - 4));

        // 5. 绘制左侧：[ ↻ 获取模型列表 ]
        const QString text = m_isRefreshing ? tr("正在获取...") : tr("获取模型列表");
        QFont font = Typography::fontStyle(Typography::FontRole::Body).toQFont();
        painter.setFont(font);
        const QFontMetrics fm(font);
        const int textWidth = fm.horizontalAdvance(text);
        const int iconSize = 14;
        const int gap = 8;
        const int totalLeftContentWidth = iconSize + gap + textWidth;
        const qreal startX = leftRect.left() + (leftRect.width() - totalLeftContentWidth) / 2.0;

        const QColor textColor = (m_isRefreshing) ? colors.textDisabled : colors.textPrimary;
        painter.setPen(textColor);

        // 绘制刷新图标
        const QRectF iconRect(startX, leftRect.top(), iconSize, leftRect.height());
        Typography::Icons::paintGlyph(painter, iconRect, Typography::Icons::Refresh, iconSize, Qt::AlignCenter);

        // 绘制文字
        const QRectF textRect(startX + iconSize + gap, leftRect.top(), textWidth + 2, leftRect.height());
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);

        // 6. 绘制右侧：[ + ]
        painter.setPen(colors.textPrimary);
        Typography::Icons::paintGlyph(painter, rightRect, Typography::Icons::Add, 14, Qt::AlignCenter);
    }

} // namespace ui::screen::settings::model_manager
